// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/thread.h"

#include "lib/ftl/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <memory>
#include <string>

#include "flutter/fml/message_loop.h"
#include "lib/ftl/synchronization/waitable_event.h"

namespace fml {

Thread::Thread(const std::string& name) : joined_(false) {
  ftl::AutoResetWaitableEvent latch;
  ftl::RefPtr<ftl::TaskRunner> runner;
  thread_ = std::make_unique<std::thread>([&latch, &runner, name]() -> void {
    SetCurrentThreadName(name);
    fml::MessageLoop::EnsureInitializedForCurrentThread();
    auto& loop = MessageLoop::GetCurrent();
    runner = loop.GetTaskRunner();
    latch.Signal();
    loop.Run();
  });
  latch.Wait();
  task_runner_ = runner;
}

Thread::~Thread() {
  Join();
}

ftl::RefPtr<ftl::TaskRunner> Thread::GetTaskRunner() const {
  return task_runner_;
}

void Thread::Join() {
  if (joined_) {
    return;
  }
  joined_ = true;
  task_runner_->PostTask([]() { MessageLoop::GetCurrent().Terminate(); });
  thread_->join();
}

#if defined(OS_WIN)
// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;
#endif

void Thread::SetCurrentThreadName(const std::string& name) {
  if (name == "") {
    return;
  }
#if OS_MACOSX
  pthread_setname_np(name.c_str());
#elif OS_LINUX || OS_ANDROID
  pthread_setname_np(pthread_self(), name.c_str());
#elif OS_WIN
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name.c_str();
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
#else
#error Unsupported Platform
#endif
}

}  // namespace fml
