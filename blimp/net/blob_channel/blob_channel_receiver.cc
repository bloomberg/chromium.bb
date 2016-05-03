// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blob_channel/blob_channel_receiver.h"

#include "base/logging.h"
#include "base/macros.h"
#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

BlobChannelReceiver::Delegate::Delegate() {}

BlobChannelReceiver::Delegate::~Delegate() {
  DCHECK(!receiver_);
}

void BlobChannelReceiver::Delegate::SetReceiver(BlobChannelReceiver* receiver) {
  receiver_ = receiver;
}

void BlobChannelReceiver::Delegate::OnBlobReceived(const BlobId& id,
                                                   BlobDataPtr data) {
  if (receiver_) {
    receiver_->OnBlobReceived(id, data);
  }
}

BlobChannelReceiver::BlobChannelReceiver(std::unique_ptr<BlobCache> cache,
                                         std::unique_ptr<Delegate> delegate)
    : cache_(std::move(cache)), delegate_(std::move(delegate)) {
  DCHECK(cache_);
  delegate_->SetReceiver(this);
}

BlobChannelReceiver::~BlobChannelReceiver() {
  delegate_->SetReceiver(nullptr);
}

BlobDataPtr BlobChannelReceiver::Get(const BlobId& id) {
  DVLOG(2) << "Get blob: " << id;

  base::AutoLock lock(cache_lock_);
  return cache_->Get(id);
}

void BlobChannelReceiver::OnBlobReceived(const BlobId& id, BlobDataPtr data) {
  DVLOG(2) << "Blob received: " << id;

  base::AutoLock lock(cache_lock_);
  cache_->Put(id, data);
}

}  // namespace blimp
