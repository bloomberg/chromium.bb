// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_service_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// A stub implementation of AttachmentService that counts the number of times
// its methods are invoked.
class StubAttachmentService : public AttachmentService,
                              public base::NonThreadSafe {
 public:
  StubAttachmentService() : call_count_(0), weak_ptr_factory_(this) {
    // DetachFromThread because we will be constructed in one thread and
    // used/destroyed in another.
    DetachFromThread();
  }

  ~StubAttachmentService() override {}

  void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) override {
    CalledOnValidThread();
    Increment();
    std::unique_ptr<AttachmentMap> attachments(new AttachmentMap());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, AttachmentService::GET_UNSPECIFIED_ERROR,
                   base::Passed(&attachments)));
  }

  void UploadAttachments(const AttachmentIdList& attachments_ids) override {
    CalledOnValidThread();
    Increment();
  }

  virtual base::WeakPtr<AttachmentService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Return the number of method invocations.
  int GetCallCount() const {
    base::AutoLock lock(mutex_);
    return call_count_;
  }

 private:
  // Protects call_count_.
  mutable base::Lock mutex_;
  int call_count_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentService> weak_ptr_factory_;

  void Increment() {
    base::AutoLock lock(mutex_);
    ++call_count_;
  }
};

class AttachmentServiceProxyTest : public testing::Test,
                                   public base::NonThreadSafe {
 protected:
  AttachmentServiceProxyTest() {}

  void SetUp() override {
    CalledOnValidThread();
    stub_thread_ =
        base::MakeUnique<base::Thread>("attachment service stub thread");
    stub_thread_->Start();
    stub_ = base::MakeUnique<StubAttachmentService>();
    proxy_ = base::MakeUnique<AttachmentServiceProxy>(
        stub_thread_->task_runner(), stub_->AsWeakPtr());

    callback_get_or_download_ =
        base::Bind(&AttachmentServiceProxyTest::IncrementGetOrDownload,
                   base::Unretained(this));
    count_callback_get_or_download_ = 0;
  }

  void TearDown() override {
    // We must take care to call the stub's destructor on the stub_thread_
    // because that's the thread to which its WeakPtrs are bound.
    if (stub_) {
      stub_thread_->task_runner()->DeleteSoon(FROM_HERE, stub_.release());
      WaitForStubThread();
    }
    stub_thread_->Stop();
  }

  // a GetOrDownloadCallback
  void IncrementGetOrDownload(const AttachmentService::GetOrDownloadResult&,
                              std::unique_ptr<AttachmentMap>) {
    CalledOnValidThread();
    ++count_callback_get_or_download_;
  }

  void WaitForStubThread() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    stub_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

  base::MessageLoop loop_;
  std::unique_ptr<base::Thread> stub_thread_;
  std::unique_ptr<StubAttachmentService> stub_;
  std::unique_ptr<AttachmentServiceProxy> proxy_;

  AttachmentService::GetOrDownloadCallback callback_get_or_download_;

  // number of times callback_get_or_download_ was invoked
  int count_callback_get_or_download_;
};

// Verify that each of AttachmentServiceProxy's methods are invoked on the stub.
// Verify that the methods that take callbacks invoke passed callbacks on this
// thread.
TEST_F(AttachmentServiceProxyTest, MethodsAreProxied) {
  proxy_->GetOrDownloadAttachments(AttachmentIdList(),
                                   callback_get_or_download_);
  proxy_->UploadAttachments(AttachmentIdList());
  // Wait for the posted calls to execute in the stub thread.
  WaitForStubThread();
  EXPECT_EQ(2, stub_->GetCallCount());
  // At this point the stub thread has finished executed the calls. However, the
  // result callbacks it has posted may not have executed yet. Wait a second
  // time to ensure the stub thread has executed the posted result callbacks.
  WaitForStubThread();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download_);
}

// Verify that it's safe to use an AttachmentServiceProxy even after its wrapped
// AttachmentService has been destroyed.
TEST_F(AttachmentServiceProxyTest, WrappedIsDestroyed) {
  proxy_->GetOrDownloadAttachments(AttachmentIdList(),
                                   callback_get_or_download_);
  // Wait for the posted calls to execute in the stub thread.
  WaitForStubThread();
  EXPECT_EQ(1, stub_->GetCallCount());
  // Wait a second time ensure the stub thread has executed the posted result
  // callbacks.
  WaitForStubThread();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download_);

  // Destroy the stub and call GetOrDownloadAttachments again.
  stub_thread_->task_runner()->DeleteSoon(FROM_HERE, stub_.release());
  WaitForStubThread();

  // Now that the wrapped object has been destroyed, call again and see that we
  // don't crash and the count remains the same.
  proxy_->GetOrDownloadAttachments(AttachmentIdList(),
                                   callback_get_or_download_);
  WaitForStubThread();
  WaitForStubThread();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, count_callback_get_or_download_);
}

}  // namespace syncer
