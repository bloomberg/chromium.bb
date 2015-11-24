// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_BUFFER_H_
#define COMPONENTS_EXO_BUFFER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {
class SingleReleaseCallback;
class TextureMailbox;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
struct SyncToken;
}

namespace exo {

// This class provides the content for a Surface. The mechanism by which a
// client provides and updates the contents is the responsibility of the client
// and not defined as part of this class.
class Buffer : public base::SupportsWeakPtr<Buffer> {
 public:
  Buffer(scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
         unsigned texture_target);
  ~Buffer();

  // Set the callback to run when the buffer is no longer used by the
  // compositor. The client is free to re-use or destroy this buffer and
  // its backing storage after this has been called.
  void set_release_callback(const base::Closure& release_callback) {
    release_callback_ = release_callback;
  }

  // This function can be used to acquire a texture mailbox that is bound to
  // the buffer. Returns a release callback on success. The release callback
  // must be called before a new texture mailbox can be acquired.
  scoped_ptr<cc::SingleReleaseCallback> ProduceTextureMailbox(
      cc::TextureMailbox* mailbox);

  // Returns the size of the buffer.
  gfx::Size GetSize() const;

  // Returns a trace value representing the state of the buffer.
  scoped_refptr<base::trace_event::TracedValue> AsTracedValue() const;

 private:
  class Texture;

  // Decrements the use count of buffer and notifies the client that buffer
  // as been released if it reached 0.
  void Release();

  // This is used by ProduceTextureMailbox() to produce a release callback
  // that releases the buffer contents referenced by a texture before the
  // texture is destroyed or reused.
  // Note: This is a static function as it needs to run even if the buffer
  // has been destroyed.
  static void ReleaseTexture(base::WeakPtr<Buffer> buffer,
                             scoped_ptr<Texture> texture,
                             const gpu::SyncToken& sync_token,
                             bool is_lost);

  // The GPU memory buffer that contains the contents of this buffer.
  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // Texture target that must be used when creating a texture for buffer.
  const unsigned texture_target_;

  // This is incremented when a texture mailbox is produced and decremented
  // when a texture mailbox is released. It is used to determine when we should
  // notify the client that buffer has been released.
  unsigned use_count_;

  // The last released texture instance. ProduceTextureMailbox() will use this
  // instead of creating a new texture when possible.
  scoped_ptr<Texture> last_texture_;

  // The client release callback.
  base::Closure release_callback_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_BUFFER_H_
