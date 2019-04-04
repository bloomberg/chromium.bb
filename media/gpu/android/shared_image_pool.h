// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SHARED_IMAGE_POOL_H_
#define MEDIA_GPU_ANDROID_SHARED_IMAGE_POOL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
class SharedImageRepresentationFactoryRef;
}  // namespace gpu

namespace media {

class CommandBufferHelper;

// TODO(vikassoni): This is a temporary class which will go away soon once all
// the video mailbox consumers switches to using shared image mailbox instead of
// legacy mailbox. Video frame will have the ownership of shared image ref
// instead of this shared image pool.
// SharedImagePool owns shared image ref. Shared image are used to hold decoded
// video frames. This pool allows shared images to outlive the decoder that
// created them, since decoders are torn down when the pipeline is suspended,
// but decoded frames can be on-screen indefinitely.
class MEDIA_GPU_EXPORT SharedImagePool
    : public base::RefCounted<SharedImagePool> {
 public:
  SharedImagePool(scoped_refptr<CommandBufferHelper> helper);

  // Add a new shared image ref into the pool.  This may only be done before
  // |stub_| is destroyed.  When |stub_| is destroyed, we will destroy/clear any
  // refs that are in the pool.
  void AddSharedImage(std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>
                          shared_image_ref);

  // Release a shared image ref back into the pool.  |shared_image_ref| must
  // have been added to the pool previously, and not released.  Otherwise, this
  // is undefined. Note: since we don't actually pool things, this just forgets
  // |shared_image_ref|. It's okay if this is called after we've lost |stub_|.
  // If |sync_token| is not null, then we'll wait for that token before taking
  // any action.
  void ReleaseSharedImage(
      gpu::SharedImageRepresentationFactoryRef* shared_image_ref,
      const gpu::SyncToken& sync_token);

 protected:
  virtual ~SharedImagePool();

  // Called after a sync token has been released, to free |shared_image_ref|.
  void OnSyncTokenReleased(
      gpu::SharedImageRepresentationFactoryRef* shared_image_ref);

  // Called when |stub_| notifies us that the underlying stub will be destroyed.
  void OnWillDestroyStub(bool have_context);

 private:
  friend class base::RefCounted<SharedImagePool>;
  THREAD_CHECKER(thread_checker_);

  scoped_refptr<CommandBufferHelper> helper_;

  base::flat_set<std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>,
                 base::UniquePtrComparator>
      pool_;

  base::WeakPtrFactory<SharedImagePool> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SHARED_IMAGE_POOL_H_
