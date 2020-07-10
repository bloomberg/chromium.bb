// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_REFLECTOR_TEXTURE_H_
#define CONTENT_BROWSER_COMPOSITOR_REFLECTOR_TEXTURE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/common/content_export.h"

namespace gfx {
class Rect;
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
}

namespace content {

// Create and manages texture mailbox to be used by Reflector.
class CONTENT_EXPORT ReflectorTexture {
 public:
  explicit ReflectorTexture(viz::ContextProvider* provider);
  ~ReflectorTexture();

  void CopyTextureFullImage(const gfx::Size& size);
  void CopyTextureSubImage(const gfx::Rect& size);

  uint32_t texture_id() const { return texture_id_; }
  scoped_refptr<OwnedMailbox> mailbox() { return mailbox_; }

 private:
  gpu::gles2::GLES2Interface* const gl_;
  scoped_refptr<OwnedMailbox> mailbox_;
  uint32_t texture_id_;

  DISALLOW_COPY_AND_ASSIGN(ReflectorTexture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_REFLECTOR_TEXTURE_H_
