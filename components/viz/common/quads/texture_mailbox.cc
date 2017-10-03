// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/texture_mailbox.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "components/viz/common/quads/shared_bitmap.h"

namespace viz {

TextureMailbox::TextureMailbox() : shared_bitmap_(NULL) {}

TextureMailbox::TextureMailbox(const TextureMailbox& other) = default;

TextureMailbox::TextureMailbox(const gpu::MailboxHolder& mailbox_holder)
    : mailbox_holder_(mailbox_holder),
      shared_bitmap_(NULL),
      is_overlay_candidate_(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture_(false),
      wants_promotion_hint_(false),
#endif
      nearest_neighbor_(false) {
}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox,
                               const gpu::SyncToken& sync_token,
                               uint32_t target)
    : mailbox_holder_(mailbox, sync_token, target),
      shared_bitmap_(NULL),
      is_overlay_candidate_(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture_(false),
      wants_promotion_hint_(false),
#endif
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
#if defined(OS_ANDROID)
      is_backed_by_surface_texture_(false),
      wants_promotion_hint_(false),
#endif
      nearest_neighbor_(false) {
  DCHECK(!is_overlay_candidate || !size_in_pixels.IsEmpty());
}

TextureMailbox::TextureMailbox(SharedBitmap* shared_bitmap,
                               const gfx::Size& size_in_pixels)
    : shared_bitmap_(shared_bitmap),
      size_in_pixels_(size_in_pixels),
      is_overlay_candidate_(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture_(false),
      wants_promotion_hint_(false),
#endif
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

}  // namespace viz
