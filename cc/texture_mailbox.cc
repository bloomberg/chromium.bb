// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/texture_mailbox.h"

namespace cc {

TextureMailbox::TextureMailbox() {
}

TextureMailbox::TextureMailbox(
    const std::string& mailbox_name,
    const ReleaseCallback& mailbox_callback)
    : callback_(mailbox_callback) {
  DCHECK(mailbox_name.empty() == mailbox_callback.is_null());
  if (!mailbox_name.empty()) {
    CHECK(mailbox_name.size() == sizeof(name_.name));
    name_.setName(reinterpret_cast<const int8*>(mailbox_name.data()));
  }
}

TextureMailbox::TextureMailbox(
    const Mailbox& mailbox_name,
    const ReleaseCallback& mailbox_callback)
    : callback_(mailbox_callback) {
  DCHECK(mailbox_name.isZero() == mailbox_callback.is_null());
  name_.setName(mailbox_name.name);
}

TextureMailbox::~TextureMailbox() {
}

bool TextureMailbox::Equals(const Mailbox& other) const {
  return !memcmp(data(), other.name, sizeof(name_.name));
}

bool TextureMailbox::Equals(const TextureMailbox& other) const {
  return Equals(other.name());
}

bool TextureMailbox::IsEmpty() const {
  return name_.isZero();
}

void TextureMailbox::RunReleaseCallback(unsigned sync_point) const {
  if (!callback_.is_null())
    callback_.Run(sync_point);
}

void TextureMailbox::SetName(const Mailbox& other) {
  name_.setName(other.name);
}

}  // namespace cc
