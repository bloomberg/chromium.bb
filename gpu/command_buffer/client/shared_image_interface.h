// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gfx {
class ColorSpace;
class GpuMemoryBuffer;
class Size;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferManager;

// An interface to create shared images that can be imported into other APIs.
// This interface is thread-safe and (essentially) stateless. It is asynchronous
// in the same sense as GLES2Interface or RasterInterface in that commands are
// executed asynchronously on the service side, but can be synchronized using
// SyncTokens. See //docs/design/gpu_synchronization.md.
class SharedImageInterface {
 public:
  virtual ~SharedImageInterface() {}

  // Creates a shared image of requested |format|, |size| and |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with.
  // Returns a mailbox that can be imported into said APIs using their
  // corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM) or (deprecated)
  // mailbox functions (e.g.  RasterInterface::CreateAndConsumeTexture or
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage) = 0;

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
  virtual Mailbox CreateSharedImage(viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage,
                                    base::span<const uint8_t> pixel_data) = 0;

  // Creates a shared image out of a GpuMemoryBuffer, using |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with. Format and size are derived from the
  // GpuMemoryBuffer. |gpu_memory_buffer_manager| is the manager that created
  // |gpu_memory_buffer|. If valid, |color_space| will be applied to the shared
  // image (possibly overwriting the one set on the GpuMemoryBuffer).
  // Returns a mailbox that can be imported into said APIs using their
  // corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM) or (deprecated)
  // mailbox functions (e.g.  RasterInterface::CreateAndConsumeTexture or
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      uint32_t usage) = 0;

  // Updates a shared image after its GpuMemoryBuffer (if any) was modified on
  // the CPU or through external devices, after |sync_token| has been released.
  virtual void UpdateSharedImage(const SyncToken& sync_token,
                                 const Mailbox& mailbox) = 0;

  // Destroys the shared image, unregistering its mailbox, after |sync_token|
  // has been released. After this call, the mailbox can't be used to reference
  // the image any more, however if the image was imported into other APIs,
  // those may keep a reference to the underlying data.
  virtual void DestroySharedImage(const SyncToken& sync_token,
                                  const Mailbox& mailbox) = 0;

  // Generates an unverified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenUnverifiedSyncToken() = 0;

  // Generates a verified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenVerifiedSyncToken() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
