// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_
#define BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlobCache;

// Receives blobs from a remote sender.
class BLIMP_NET_EXPORT BlobChannelReceiver {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

   protected:
    // Forwards incoming blob data to |receiver_|.
    void OnBlobReceived(const BlobId& id, BlobDataPtr data);

   private:
    friend class BlobChannelReceiver;

    // Sets the Receiver object which will take incoming blobs.
    void SetReceiver(BlobChannelReceiver* receiver);

    BlobChannelReceiver* receiver_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  BlobChannelReceiver(std::unique_ptr<BlobCache> cache,
                      std::unique_ptr<Delegate> delegate);
  ~BlobChannelReceiver();

  // Gets a blob from the BlobChannel.
  // Returns nullptr if the blob is not available in the channel.
  // Can be accessed concurrently from any thread. Calling code must ensure that
  // the object instance outlives all calls to Get().
  BlobDataPtr Get(const BlobId& id);

 private:
  friend class Delegate;

  // Called by Delegate::OnBlobReceived() when a blob arrives over the channel.
  void OnBlobReceived(const BlobId& id, BlobDataPtr data);

  std::unique_ptr<BlobCache> cache_;
  std::unique_ptr<Delegate> delegate_;

  // Guards against concurrent access to |cache_|.
  base::Lock cache_lock_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelReceiver);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLOB_CHANNEL_BLOB_CHANNEL_RECEIVER_H_
