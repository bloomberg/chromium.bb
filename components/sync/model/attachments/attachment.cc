// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment.h"

#include "base/logging.h"
#include "components/sync/engine/attachments/attachment_util.h"

namespace syncer {

Attachment::Attachment(const Attachment& other) = default;

Attachment::~Attachment() {}

// Static.
Attachment Attachment::Create(
    const scoped_refptr<base::RefCountedMemory>& data) {
  uint32_t crc32c = ComputeCrc32c(data);
  return CreateFromParts(AttachmentId::Create(data->size(), crc32c), data);
}

// Static.
Attachment Attachment::CreateFromParts(
    const AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return Attachment(id, data);
}

const AttachmentId& Attachment::GetId() const {
  return id_;
}

const scoped_refptr<base::RefCountedMemory>& Attachment::GetData() const {
  return data_;
}

uint32_t Attachment::GetCrc32c() const {
  return id_.GetCrc32c();
}

Attachment::Attachment(const AttachmentId& id,
                       const scoped_refptr<base::RefCountedMemory>& data)
    : id_(id), data_(data) {
  DCHECK_EQ(id.GetSize(), data->size());
  DCHECK(data.get());
}

}  // namespace syncer
