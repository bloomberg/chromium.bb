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
  DestroyAllPlatformTextures();

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

void TexturePool::DestroyAllPlatformTextures() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Don't bother to make the context current if we have no textures.
  if (!pool_.size())
    return;

  // If we can't make the context current, then notify all the textures that
  // they can't delete the underlying platform textures.
  const bool have_context = stub_ && stub_->MakeCurrent();

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

void TexturePool::OnWillDestroyStub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  // Since the stub is going away, clean up while we can.
  DestroyAllPlatformTextures();
  stub_ = nullptr;
}

}  // namespace media
