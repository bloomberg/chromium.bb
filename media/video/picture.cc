// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "media/video/picture.h"

namespace media {

PictureBuffer::PictureBuffer(int32_t id,
                             gfx::Size size,
                             const TextureIds& client_texture_ids)
    : id_(id), size_(size), client_texture_ids_(client_texture_ids) {}

PictureBuffer::PictureBuffer(int32_t id,
                             gfx::Size size,
                             const TextureIds& client_texture_ids,
                             const TextureIds& service_texture_ids)
    : id_(id),
      size_(size),
      client_texture_ids_(client_texture_ids),
      service_texture_ids_(service_texture_ids) {
  DCHECK_EQ(client_texture_ids.size(), service_texture_ids.size());
}

PictureBuffer::PictureBuffer(int32_t id,
                             gfx::Size size,
                             const TextureIds& client_texture_ids,
                             const std::vector<gpu::Mailbox>& texture_mailboxes)
    : id_(id),
      size_(size),
      client_texture_ids_(client_texture_ids),
      texture_mailboxes_(texture_mailboxes) {
  DCHECK_EQ(client_texture_ids.size(), texture_mailboxes.size());
}

PictureBuffer::PictureBuffer(const PictureBuffer& other) = default;

PictureBuffer::~PictureBuffer() {}

Picture::Picture(int32_t picture_buffer_id,
                 int32_t bitstream_buffer_id,
                 const gfx::Rect& visible_rect,
                 const gfx::ColorSpace& color_space,
                 bool allow_overlay)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      visible_rect_(visible_rect),
      color_space_(color_space),
      allow_overlay_(allow_overlay),
      size_changed_(false) {}

}  // namespace media
