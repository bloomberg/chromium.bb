// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/texture_mailbox.h"

namespace cc {

TextureMailbox::TextureMailbox()
    : sync_point_(0) {
}

TextureMailbox::TextureMailbox(
    const std::string& mailbox_name,
    const ReleaseCallback& mailbox_callback)
    : callback_(mailbox_callback),
      sync_point_(0) {
  DCHECK(mailbox_name.empty() == mailbox_callback.is_null());
  if (!mailbox_name.empty()) {
    CHECK(mailbox_name.size() == sizeof(name_.name));
    name_.SetName(reinterpret_cast<const int8*>(mailbox_name.data()));
  }
}

TextureMailbox::TextureMailbox(
    const gpu::Mailbox& mailbox_name,
    const ReleaseCallback& mailbox_callback)
    : callback_(mailbox_callback),
      sync_point_(0) {
  DCHECK(mailbox_name.IsZero() == mailbox_callback.is_null());
  name_.SetName(mailbox_name.name);
}

TextureMailbox::TextureMailbox(
    const gpu::Mailbox& mailbox_name,
    const ReleaseCallback& mailbox_callback,
    unsigned sync_point)
    : callback_(mailbox_callback),
      sync_point_(sync_point) {
  DCHECK(mailbox_name.IsZero() == mailbox_callback.is_null());
  name_.SetName(mailbox_name.name);
}

TextureMailbox::~TextureMailbox() {
}

bool TextureMailbox::Equals(const gpu::Mailbox& other) const {
  return !memcmp(data(), other.name, sizeof(name_.name));
}

bool TextureMailbox::Equals(const TextureMailbox& other) const {
  return Equals(other.name());
}

bool TextureMailbox::IsEmpty() const {
  return name_.IsZero();
}

void TextureMailbox::RunReleaseCallback(unsigned sync_point) const {
  if (!callback_.is_null())
    callback_.Run(sync_point);
}

void TextureMailbox::SetName(const gpu::Mailbox& other) {
  name_.SetName(other.name);
}

}  // namespace cc
