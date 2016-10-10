// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_metadata.h"

namespace syncer {

AttachmentMetadata::AttachmentMetadata(const AttachmentId& id, size_t size)
    : id_(id), size_(size) {}

AttachmentMetadata::~AttachmentMetadata() {}

const AttachmentId& AttachmentMetadata::GetId() const {
  return id_;
}

size_t AttachmentMetadata::GetSize() const {
  return size_;
}

}  // namespace syncer
