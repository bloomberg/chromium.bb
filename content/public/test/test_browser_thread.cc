// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/notification_service_impl.h"

namespace content {

class TestBrowserThreadImpl : public BrowserThreadImpl {
 public:
  explicit TestBrowserThreadImpl(BrowserThread::ID identifier)
      : BrowserThreadImpl(identifier),
        notification_service_(NULL) {
  }

  TestBrowserThreadImpl(BrowserThread::ID identifier,
                        base::MessageLoop* message_loop)
      : BrowserThreadImpl(identifier, message_loop),
        notification_service_(NULL) {}

  virtual ~TestBrowserThreadImpl() {
    Stop();
  }

  virtual void Init() OVERRIDE {
    notification_service_ = new NotificationServiceImpl;
    BrowserThreadImpl::Init();
  }

  virtual void CleanUp() OVERRIDE {
    delete notification_service_;
    notification_service_ = NULL;
    BrowserThreadImpl::CleanUp();
  }

 private:
  NotificationService* notification_service_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadImpl);
};

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier)
    : impl_(new TestBrowserThreadImpl(identifier)) {
}

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier,
                                     base::MessageLoop* message_loop)
    : impl_(new TestBrowserThreadImpl(identifier, message_loop)) {}

TestBrowserThread::~TestBrowserThread() {
  Stop();
}

bool TestBrowserThread::Start() {
  return impl_->Start();
}

bool TestBrowserThread::StartIOThread() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
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
