// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/notification_service_impl.h"

namespace content {

class TestBrowserThreadImpl : public BrowserThreadImpl {
 public:
  explicit TestBrowserThreadImpl(BrowserThread::ID identifier)
      : BrowserThreadImpl(identifier) {}

  TestBrowserThreadImpl(BrowserThread::ID identifier,
                        base::MessageLoop* message_loop)
      : BrowserThreadImpl(identifier, message_loop) {}

  ~TestBrowserThreadImpl() override { Stop(); }

  void Init() override {
    notification_service_.reset(new NotificationServiceImpl);
    BrowserThreadImpl::Init();
  }

  void CleanUp() override {
    notification_service_.reset();
    BrowserThreadImpl::CleanUp();
  }

 private:
  std::unique_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadImpl);
};

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier)
    : impl_(new TestBrowserThreadImpl(identifier)), identifier_(identifier) {}

TestBrowserThread::TestBrowserThread(BrowserThread::ID identifier,
                                     base::MessageLoop* message_loop)
    : impl_(new TestBrowserThreadImpl(identifier, message_loop)),
      identifier_(identifier) {}

TestBrowserThread::~TestBrowserThread() {
  // The upcoming BrowserThreadImpl::ResetGlobalsForTesting() call requires that
  // |impl_| have triggered the shutdown phase for its BrowserThread::ID. This
  // either happens when the thread is stopped (if real) or destroyed (when fake
  // -- i.e. using an externally provided MessageLoop).
  impl_.reset();

  // Resets BrowserThreadImpl's globals so that |impl_| is no longer bound to
  // |identifier_|. This is fine since the underlying MessageLoop has already
  // been flushed and deleted in Stop(). In the case of an externally provided
  // MessageLoop however, this means that TaskRunners obtained through
  // |BrowserThreadImpl::GetTaskRunnerForThread(identifier_)| will no longer
  // recognize their BrowserThreadImpl for RunsTasksOnCurrentThread(). This
  // happens most often when such verifications are made from
  // MessageLoop::DestructionObservers. Callers that care to work around that
  // should instead use this shutdown sequence:
  //   1) TestBrowserThread::Stop()
  //   2) ~MessageLoop()
  //   3) ~TestBrowserThread()
  // (~TestBrowserThreadBundle() does this).
  BrowserThreadImpl::ResetGlobalsForTesting(identifier_);
}

bool TestBrowserThread::Start() {
  return impl_->Start();
}

bool TestBrowserThread::StartAndWaitForTesting() {
  return impl_->StartAndWaitForTesting();
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

}  // namespace content
