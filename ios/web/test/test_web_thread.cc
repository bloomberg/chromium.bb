// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "ios/web/web_thread_impl.h"

namespace web {

class TestWebThreadImpl : public WebThreadImpl {
 public:
  TestWebThreadImpl(WebThread::ID identifier) : WebThreadImpl(identifier) {}

  TestWebThreadImpl(WebThread::ID identifier, base::MessageLoop* message_loop)
      : WebThreadImpl(identifier, message_loop) {}

  ~TestWebThreadImpl() override { Stop(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebThreadImpl);
};

TestWebThread::TestWebThread(WebThread::ID identifier)
    : impl_(new TestWebThreadImpl(identifier)), identifier_(identifier) {}

TestWebThread::TestWebThread(WebThread::ID identifier,
                             base::MessageLoop* message_loop)
    : impl_(new TestWebThreadImpl(identifier, message_loop)),
      identifier_(identifier) {}

TestWebThread::~TestWebThread() {
  // The upcoming WebThreadImpl::ResetGlobalsForTesting() call requires that
  // |impl_| have triggered the shutdown phase for its WebThread::ID. This
  // either happens when the thread is stopped (if real) or destroyed (when fake
  // -- i.e. using an externally provided MessageLoop).
  impl_.reset();

  // Resets WebThreadImpl's globals so that |impl_| is no longer bound to
  // |identifier_|. This is fine since the underlying MessageLoop has already
  // been flushed and deleted in Stop(). In the case of an externally provided
  // MessageLoop however, this means that TaskRunners obtained through
  // |WebThreadImpl::GetTaskRunnerForThread(identifier_)| will no longer
  // recognize their WebThreadImpl for RunsTasksInCurrentSequence(). This
  // happens most often when such verifications are made from
  // MessageLoop::DestructionObservers. Callers that care to work around that
  // should instead use this shutdown sequence:
  //   1) TestWebThread::Stop()
  //   2) ~MessageLoop()
  //   3) ~TestWebThread()
  // (~TestWebThreadBundle() does this).
  WebThreadImpl::ResetGlobalsForTesting(identifier_);
}

bool TestWebThread::Start() {
  return impl_->Start();
}

bool TestWebThread::StartIOThread() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  return impl_->StartWithOptions(options);
}

void TestWebThread::Stop() {
  impl_->Stop();
}

bool TestWebThread::IsRunning() {
  return impl_->IsRunning();
}

}  // namespace web
