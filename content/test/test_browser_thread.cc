// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_browser_thread.h"

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"

namespace content {

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier)
    : impl_(new BrowserThreadImpl(identifier)) {
}

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier,
                                     MessageLoop* message_loop)
    : impl_(new BrowserThreadImpl(identifier, message_loop)) {
}

TestBrowserThread::~TestBrowserThread() {
  Stop();
}

bool TestBrowserThread::Start() {
  return impl_->Start();
}

bool TestBrowserThread::StartIOThread() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  return impl_->StartWithOptions(options);
}

void TestBrowserThread::Stop() {
  impl_->Stop();
}

bool TestBrowserThread::IsRunning() {
  return impl_->IsRunning();
}

base::Thread* TestBrowserThread::DeprecatedGetThreadObject() {
  return impl_.get();
}

}  // namespace content
