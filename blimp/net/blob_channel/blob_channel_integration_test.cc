// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/common/blob_cache/test_util.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "blimp/net/test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

using testing::_;
using testing::SaveArg;

const char kBlobId[] = "foo1";
const char kBlobPayload[] = "bar1";

// Routes sender delegate calls to a receiver delegate object.
// The caller is responsible for ensuring that the receiver delegate is deleted
// after |this| is deleted.
class SenderDelegateProxy : public BlobChannelSender::Delegate {
 public:
  SenderDelegateProxy() {}
  ~SenderDelegateProxy() override {}

  // Returns a receiver object that will receive proxied calls sent to |this|.
  std::unique_ptr<BlobChannelReceiver::Delegate> GetReceiverDelegate() {
    DCHECK(!receiver_);
    receiver_ = new ReceiverDelegate;
    return base::WrapUnique(receiver_);
  }

 private:
  class ReceiverDelegate : public BlobChannelReceiver::Delegate {
   public:
    using BlobChannelReceiver::Delegate::OnBlobReceived;
  };

  // BlobChannelSender implementation.
  void DeliverBlob(const BlobId& id, BlobDataPtr data) override {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ReceiverDelegate::OnBlobReceived,
                              base::Unretained(receiver_), id, data));
  }

  ReceiverDelegate* receiver_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SenderDelegateProxy);
};

// Verifies compatibility between the sender and receiver, independent of
// transport-level implementation details.
class BlobChannelIntegrationTest : public testing::Test {
 public:
  BlobChannelIntegrationTest() {
    std::unique_ptr<SenderDelegateProxy> sender_delegate(
        new SenderDelegateProxy);
    receiver_.reset(
        new BlobChannelReceiver(base::WrapUnique(new InMemoryBlobCache),
                                sender_delegate->GetReceiverDelegate()));
    sender_.reset(new BlobChannelSender(base::WrapUnique(new InMemoryBlobCache),
                                        std::move(sender_delegate)));
  }

  ~BlobChannelIntegrationTest() override {}

 protected:
  std::unique_ptr<BlobChannelReceiver> receiver_;
  std::unique_ptr<BlobChannelSender> sender_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobChannelIntegrationTest);
};

TEST_F(BlobChannelIntegrationTest, Deliver) {
  const std::string blob_id = CalculateBlobId(kBlobId);

  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());
  sender_->PutBlob(blob_id, CreateBlobDataPtr(kBlobPayload));
  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());

  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());
  sender_->DeliverBlob(blob_id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kBlobPayload, receiver_->Get(blob_id)->data);
}

}  // namespace
}  // namespace blimp
