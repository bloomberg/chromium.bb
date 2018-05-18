// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_pool.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "media/gpu/android/command_buffer_stub_wrapper.h"
#include "media/gpu/android/texture_wrapper.h"

namespace media {

TexturePool::TexturePool(std::unique_ptr<CommandBufferStubWrapper> stub)
    : stub_(std::move(stub)) {
  if (stub_)
    stub_->AddDestructionObserver(this);
}

TexturePool::~TexturePool() {
  // Note that the size of |pool_| doesn't, in general, tell us if there are any
  // textures.  If the stub has been destroyed, then we will drop the
  // TextureRefs but leave null entries in the map.  So, we check |stub_| too.
  if (pool_.size() && stub_) {
    // TODO(liberato): consider using ScopedMakeCurrent here, though if we are
    // ever called as part of decoder teardown, then using ScopedMakeCurrent
    // isn't safe.  For now, we preserve the old behavior (MakeCurrent).
    //
    // We check IsCurrent, even though that only checks for the underlying
    // shared context if |context| is a virtual context.  Assuming that all
    // TextureRef does is to delete a texture, this is enough.  Of course, we
    // shouldn't assume that this is all it does.
    bool have_context = stub_->IsCurrent() || stub_->MakeCurrent();
    DestroyAllPlatformTextures(have_context);
  }

  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

void TexturePool::AddTexture(std::unique_ptr<TextureWrapper> texture) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(pool_.find(texture.get()) == pool_.end());
  // Don't permit additions after we've lost the stub.
  // TODO(liberato): consider making this fail gracefully.  However, nobody
  // should be doing this, so for now it's a DCHECK.
  DCHECK(stub_);
  TextureWrapper* texture_raw = texture.get();
  pool_[texture_raw] = std::move(texture);
}

void TexturePool::ReleaseTexture(TextureWrapper* texture) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = pool_.find(texture);
  DCHECK(iter != pool_.end());

  // If we can't make the context current, then notify the texture.  Note that
  // the wrapper might already have been destroyed, which is fine.
  if (iter->second && (!stub_ || !stub_->MakeCurrent()))
    texture->ForceContextLost();

  pool_.erase(iter);
}

void TexturePool::DestroyAllPlatformTextures(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Destroy the wrapper, but keep the entry around in the map.  We do this so
  // that ReleaseTexture can still check that at least the texture was, at some
  // point, in the map.  Hopefully, since nobody should be adding textures to
  // the pool after we've lost the stub, there's no issue with aliasing if the
  // ptr is re-used; it won't be given to us, so it's okay.
  for (auto& it : pool_) {
    std::unique_ptr<TextureWrapper> texture = std::move(it.second);
    if (!texture)
      continue;

    if (!have_context)
      texture->ForceContextLost();

    // |texture| will be destroyed.
  }
}

void TexturePool::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  DestroyAllPlatformTextures(have_context);
  stub_ = nullptr;
}

}  // namespace media
