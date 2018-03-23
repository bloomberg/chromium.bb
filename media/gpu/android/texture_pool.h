// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_TEXTURE_POOL_H_
#define MEDIA_GPU_ANDROID_TEXTURE_POOL_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class CommandBufferStubWrapper;
class TextureWrapper;

// Owns Textures that are used to hold decoded video frames.  Allows them to
// outlive the decoder that created them, since decoders are torn down when the
// pipeline is suspended, but decoded frames can be on-screen indefinitely.
// TODO(tmathmeyer): Convert this into a pool.  Right now, we just constantly
// add new textures and remove them.
class MEDIA_GPU_EXPORT TexturePool
    : public base::RefCounted<TexturePool>,
      public gpu::CommandBufferStub::DestructionObserver {
 public:
  TexturePool(std::unique_ptr<CommandBufferStubWrapper> stub);

  // Add a new texture into the pool.  This may only be done before |stub_| is
  // destroyed.  When |stub_| is destroyed, we will destroy any textures that
  // are in the pool.
  //
  // Note that if we were really a pool this would mean "add |texture| into the
  // pool of available textures".  There would be some other call to allocate
  // a texture from the pool.
  void AddTexture(std::unique_ptr<TextureWrapper> texture);

  // Release a texture back into the pool.  |texture| must have been added to
  // the pool previously, and not released.  Otherwise, this is undefined.
  // Note: since we don't actually pool things, this just forgets |texture|.
  // It's okay if this is called after we've lost |stub_|.
  void ReleaseTexture(TextureWrapper* texture);

 protected:
  ~TexturePool() override;

  // DestructionObserver
  void OnWillDestroyStub() override;

  // When called, we will destroy any platform textures if we have a context,
  // or mark them as "lost context" if we don't.  This will not actually remove
  // entries in |pool_|, but will instead clear the unique_ptr to delete the
  // texture.  Assuming that nobody adds textures after our stub is destroyed,
  // this is still alias-free.
  void DestroyAllPlatformTextures();

 private:
  friend class base::RefCounted<TexturePool>;
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CommandBufferStubWrapper> stub_;

  std::map<TextureWrapper*, std::unique_ptr<TextureWrapper>> pool_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_TEXTURE_POOL_H_
