// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TEXTURE_MAILBOX_H_
#define CC_RESOURCES_TEXTURE_MAILBOX_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/memory/shared_memory.h"
#include "cc/base/cc_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace mojom {
class TextureMailboxDataView;
}

class SharedBitmap;

// TODO(skaslev, danakj) Rename this class more apropriately since now it
// can hold a shared memory resource as well as a texture mailbox.
class CC_EXPORT TextureMailbox {
 public:
  TextureMailbox();
  TextureMailbox(const TextureMailbox& other);
  explicit TextureMailbox(const gpu::MailboxHolder& mailbox_holder);
  TextureMailbox(const gpu::Mailbox& mailbox,
                 const gpu::SyncToken& sync_token,
                 uint32_t target);
  TextureMailbox(const gpu::Mailbox& mailbox,
                 const gpu::SyncToken& sync_token,
                 uint32_t target,
                 const gfx::Size& size_in_pixels,
                 bool is_overlay_candidate,
                 bool secure_output_only);
  TextureMailbox(SharedBitmap* shared_bitmap, const gfx::Size& size_in_pixels);

  ~TextureMailbox();

  bool IsValid() const { return IsTexture() || IsSharedMemory(); }
  bool IsTexture() const { return !mailbox_holder_.mailbox.IsZero(); }
  bool IsSharedMemory() const { return shared_bitmap_ != NULL; }
  bool HasSyncToken() const { return mailbox_holder_.sync_token.HasData(); }

  int8_t* GetSyncTokenData() { return mailbox_holder_.sync_token.GetData(); }

  bool Equals(const TextureMailbox&) const;

  const gpu::Mailbox& mailbox() const { return mailbox_holder_.mailbox; }
  const int8_t* name() const { return mailbox().name; }
  uint32_t target() const { return mailbox_holder_.texture_target; }
  const gpu::SyncToken& sync_token() const {
    return mailbox_holder_.sync_token;
  }
  void set_sync_token(const gpu::SyncToken& sync_token) {
    mailbox_holder_.sync_token = sync_token;
  }

  bool is_overlay_candidate() const { return is_overlay_candidate_; }
  void set_is_overlay_candidate(bool overlay_candidate) {
    is_overlay_candidate_ = overlay_candidate;
  }
  bool secure_output_only() const { return secure_output_only_; }
  bool nearest_neighbor() const { return nearest_neighbor_; }
  void set_nearest_neighbor(bool nearest_neighbor) {
    nearest_neighbor_ = nearest_neighbor;
  }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  void set_color_space(const gfx::ColorSpace& color_space) {
    color_space_ = color_space;
  }

  // This is valid if allow_overlau() or IsSharedMemory() is true.
  gfx::Size size_in_pixels() const { return size_in_pixels_; }

  SharedBitmap* shared_bitmap() const { return shared_bitmap_; }
  size_t SharedMemorySizeInBytes() const;

#if defined(OS_ANDROID)
  bool is_backed_by_surface_texture() const {
    return is_backed_by_surface_texture_;
  }

  void set_is_backed_by_surface_texture(bool value) {
    is_backed_by_surface_texture_ = value;
  }

  bool wants_promotion_hint() const { return wants_promotion_hint_; }

  void set_wants_promotion_hint(bool value) { wants_promotion_hint_ = value; }
#endif

 private:
  friend struct mojo::StructTraits<mojom::TextureMailboxDataView,
                                   TextureMailbox>;

  gpu::MailboxHolder mailbox_holder_;
  SharedBitmap* shared_bitmap_;
  gfx::Size size_in_pixels_;
  bool is_overlay_candidate_;
#if defined(OS_ANDROID)
  bool is_backed_by_surface_texture_;
  bool wants_promotion_hint_;
#endif
  bool secure_output_only_;
  bool nearest_neighbor_;
  gfx::ColorSpace color_space_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TEXTURE_MAILBOX_H_
