// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_browser_thread.h"

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"

namespace content {

// This gives access to set_message_loop().
class TestBrowserThreadImpl : public BrowserThreadImpl {
 public:
  explicit TestBrowserThreadImpl(BrowserThread::ID identifier)
      : BrowserThreadImpl(identifier) {
  }

  TestBrowserThreadImpl(BrowserThread::ID identifier,
                        MessageLoop* message_loop)
      : BrowserThreadImpl(identifier, message_loop) {
  }

  virtual ~TestBrowserThreadImpl() {
    Stop();
  }

  void set_message_loop(MessageLoop* loop) {
    Thread::set_message_loop(loop);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadImpl);
};

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier)
    : impl_(new TestBrowserThreadImpl(identifier)) {
}

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier,
                                     MessageLoop* message_loop)
    : impl_(new TestBrowserThreadImpl(identifier, message_loop)) {
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

void TestBrowserThread::DeprecatedSetMessageLoop(MessageLoop* loop) {
  impl_->set_message_loop(loop);
}

}  // namespace content
