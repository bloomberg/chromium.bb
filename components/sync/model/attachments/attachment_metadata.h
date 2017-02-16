// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_METADATA_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_METADATA_H_

#include <stddef.h>

#include <vector>

#include "components/sync/model/attachments/attachment_id.h"

namespace syncer {

// This class represents immutable Attachment metadata.
//
// It is OK to copy and return AttachmentMetadata by value.
class AttachmentMetadata {
 public:
  AttachmentMetadata(const AttachmentId& id, size_t size);
  ~AttachmentMetadata();

  // Default copy and assignment are welcome.

  // Returns this attachment's id.
  const AttachmentId& GetId() const;

  // Returns this attachment's size in bytes.
  size_t GetSize() const;

 private:
  // TODO(maniscalco): Reconcile AttachmentMetadata and
  // AttachmentId. AttachmentId knows the size of the attachment so
  // AttachmentMetadata may not be necessary (crbug/465375).
  AttachmentId id_;
  size_t size_;
};

using AttachmentMetadataList = std::vector<AttachmentMetadata>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_METADATA_H_
