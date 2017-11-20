// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/texture_mailbox.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace viz {

TextureMailbox::TextureMailbox() : shared_bitmap_(nullptr) {}

TextureMailbox::TextureMailbox(const TextureMailbox& other) = default;

TextureMailbox::TextureMailbox(const gpu::MailboxHolder& mailbox_holder)
    : mailbox_holder_(mailbox_holder),
      shared_bitmap_(nullptr),
      is_overlay_candidate_(false),
      nearest_neighbor_(false) {
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox,
                               const gpu::SyncToken& sync_token,
                               uint32_t target)
    : mailbox_holder_(mailbox, sync_token, target),
      shared_bitmap_(nullptr),
      is_overlay_candidate_(false),
      nearest_neighbor_(false) {
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox,
                               const gpu::SyncToken& sync_token,
                               uint32_t target,
                               const gfx::Size& size_in_pixels,
                               bool is_overlay_candidate)
    : mailbox_holder_(mailbox, sync_token, target),
      shared_bitmap_(nullptr),
      size_in_pixels_(size_in_pixels),
      is_overlay_candidate_(is_overlay_candidate),
      nearest_neighbor_(false) {
  DCHECK(!is_overlay_candidate || !size_in_pixels.IsEmpty());
}

TextureMailbox::TextureMailbox(SharedBitmap* shared_bitmap,
                               const gfx::Size& size_in_pixels)
    : shared_bitmap_(shared_bitmap),
      size_in_pixels_(size_in_pixels),
      is_overlay_candidate_(false),
      nearest_neighbor_(false) {
  // If an embedder of cc gives an invalid TextureMailbox, we should crash
  // here to identify the offender.
  CHECK(SharedBitmap::VerifySizeInBytes(size_in_pixels_));
}

TextureMailbox::~TextureMailbox() {}

bool TextureMailbox::Equals(const TextureMailbox& other) const {
  if (other.IsTexture()) {
    return IsTexture() && !memcmp(mailbox_holder_.mailbox.name,
                                  other.mailbox_holder_.mailbox.name,
                                  sizeof(mailbox_holder_.mailbox.name));
  } else if (other.IsSharedMemory()) {
    return IsSharedMemory() && (shared_bitmap_ == other.shared_bitmap_);
  }

  DCHECK(!other.IsValid());
  return !IsValid();
}

size_t TextureMailbox::SharedMemorySizeInBytes() const {
  // UncheckedSizeInBytes is okay because we VerifySizeInBytes in the
  // constructor and the field is immutable.
  return SharedBitmap::UncheckedSizeInBytes(size_in_pixels_);
}

TransferableResource TextureMailbox::ToTransferableResource() const {
  DCHECK(IsValid());

  TransferableResource resource;
  if (IsTexture()) {
    GLuint filter = nearest_neighbor() ? GL_NEAREST : GL_LINEAR;

    if (!is_overlay_candidate()) {
      resource = TransferableResource::MakeGL(mailbox(), filter, target(),
                                              sync_token());
    } else {
      resource = TransferableResource::MakeGLOverlay(
          mailbox(), filter, target(), sync_token(), size_in_pixels(),
          is_overlay_candidate());
    }
  } else {
    resource = TransferableResource::MakeSoftware(
        shared_bitmap()->id(), shared_bitmap()->sequence_number(),
        size_in_pixels());
  }
  resource.color_space = color_space();

  return resource;
}

}  // namespace viz
