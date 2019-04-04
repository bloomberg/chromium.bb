// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_pool.h"

#include <utility>

#include "base/bind.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "media/gpu/command_buffer_helper.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

SharedImagePool::SharedImagePool(scoped_refptr<CommandBufferHelper> helper)
    : helper_(std::move(helper)), weak_factory_(this) {
  if (helper_) {
    helper_->SetWillDestroyStubCB(base::BindOnce(
        &SharedImagePool::OnWillDestroyStub, weak_factory_.GetWeakPtr()));
  }
}

SharedImagePool::~SharedImagePool() {
  // All the refs should have been dropped by this time and pool should be
  // empty. This is because video frames keep a ref of pool and once all frames
  // are released, then only pool will get destructed.
  DCHECK(pool_.empty());
}

void SharedImagePool::AddSharedImage(
    std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>
        shared_image_ref) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(pool_.find(shared_image_ref.get()) == pool_.end());
  // Don't permit additions after we've lost the stub.
  // TODO(liberato): consider making this fail gracefully.  However, nobody
  // should be doing this, so for now it's a DCHECK.
  DCHECK(helper_);
  pool_.insert(std::move(shared_image_ref));
}

void SharedImagePool::ReleaseSharedImage(
    gpu::SharedImageRepresentationFactoryRef* shared_image_ref,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If we don't have a sync token, or if we have no stub, then just finish.
  if (!sync_token.HasData() || !helper_) {
    OnSyncTokenReleased(shared_image_ref);
    return;
  }

  // We keep a strong ref to |this| in the callback, so that we are guaranteed
  // to receive it.  It's common for the last ref to us to be our caller, as
  // a callback.  We need to stick around a bit longer than that if there's a
  // sync token.  Plus, we're required to keep |helper_| around while a wait is
  // still pending.
  helper_->WaitForSyncToken(
      sync_token,
      base::BindOnce(&SharedImagePool::OnSyncTokenReleased,
                     scoped_refptr<SharedImagePool>(this), shared_image_ref));
}

void SharedImagePool::OnSyncTokenReleased(
    gpu::SharedImageRepresentationFactoryRef* shared_image_ref) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If there is no |helper_|, it means stub has been destroyed and pool has
  // been already cleared.
  if (!helper_) {
    DCHECK(pool_.empty());
    return;
  }
  auto iter = pool_.find(shared_image_ref);
  DCHECK(iter != pool_.end());

  // Drop the shared_image_ref.  This is safe without the context being current.
  // It's also safe if the stub has been destroyed.
  pool_.erase(iter);
}

void SharedImagePool::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(helper_);

  // Clearing the pools clears up all the refs which in turn releases all the
  // codec buffers.
  pool_.clear();
  helper_ = nullptr;
}

}  // namespace media
