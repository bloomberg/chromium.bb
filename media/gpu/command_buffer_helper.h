// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_COMMAND_BUFFER_HELPER_H_
#define MEDIA_GPU_COMMAND_BUFFER_HELPER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

// TODO(sandersd): CommandBufferHelper does not inherently need to be ref
// counted, but some clients want that (VdaVideoDecoder and PictureBufferManager
// both hold a ref to the same CommandBufferHelper). Consider making an owned
// variant.
class MEDIA_GPU_EXPORT CommandBufferHelper
    : public base::RefCountedThreadSafe<CommandBufferHelper> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // TODO(sandersd): Consider adding an Initialize(stub) method so that
  // CommandBufferHelpers can be created before a stub is available.
  static scoped_refptr<CommandBufferHelper> Create(
      gpu::CommandBufferStub* stub);

  virtual bool MakeContextCurrent() = 0;

  // Creates a texture and returns its |service_id|.
  //
  // See glTexImage2D() for argument definitions.
  //
  // The texture will be configured as a video frame: linear filtering, clamp to
  // edge, no mipmaps. If |target| is GL_TEXTURE_2D, storage will be allocated
  // but not initialized.
  //
  // The context must be current.
  //
  // TODO(sandersd): Is really necessary to allocate storage? GpuVideoDecoder
  // does this, but it's not clear that any clients require it.
  virtual GLuint CreateTexture(GLenum target,
                               GLenum internal_format,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type) = 0;

  // Destroys a texture.
  //
  // The context must be current.
  virtual void DestroyTexture(GLuint service_id) = 0;

  // Creates a mailbox for a texture.
  //
  // TODO(sandersd): Specify the behavior when the stub has been destroyed. The
  // current implementation returns an empty (zero) mailbox. One solution would
  // be to add a HasStub() method, and not define behavior when it is false.
  virtual gpu::Mailbox CreateMailbox(GLuint service_id) = 0;

  // Marks layer 0 of the texture as cleared.
  virtual void SetCleared(GLuint service_id) = 0;

  // Waits for a SyncToken, then runs |done_cb|.
  //
  // |done_cb| may be destructed without running if the stub is destroyed.
  //
  // TODO(sandersd): Currently it is possible to lose the stub while
  // PictureBufferManager is waiting for all picture buffers, which results in a
  // decoding softlock. Notification of wait failure (or just context/stub lost)
  // is probably necessary.
  virtual void WaitForSyncToken(gpu::SyncToken sync_token,
                                base::OnceClosure done_cb) = 0;

 protected:
  CommandBufferHelper() = default;

  // TODO(sandersd): Deleting remaining textures upon destruction requires
  // making the context current, which may be undesireable. Consider adding an
  // explicit DestroyWithContext() API.
  virtual ~CommandBufferHelper() = default;

 private:
  friend class base::RefCountedThreadSafe<CommandBufferHelper>;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelper);
};

}  // namespace media

#endif  // MEDIA_GPU_COMMAND_BUFFER_HELPER_H_
