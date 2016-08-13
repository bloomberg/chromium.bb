// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_ATTACHMENT_ID_PROTO_H_
#define COMPONENTS_SYNC_BASE_ATTACHMENT_ID_PROTO_H_

#include <stddef.h>
#include <stdint.h>

#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// Helper functions that are logically part of sync_pb::AttachmentIdProto.

// Creates a unique AttachmentIdProto.
//
// |size| is the size in bytes of the attachment identified by this proto.
//
// |crc32c| is the crc32c of the attachment identified by this proto.
sync_pb::AttachmentIdProto CreateAttachmentIdProto(size_t size,
                                                   uint32_t crc32c);

// Creates an AttachmentMetadata object from a repeated field of
// AttachmentIdProto objects.
//
// Note: each record in the AttachmentMetadata will be marked as "on the
// server".
sync_pb::AttachmentMetadata CreateAttachmentMetadata(
    const google::protobuf::RepeatedPtrField<sync_pb::AttachmentIdProto>& ids);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_ATTACHMENT_ID_PROTO_H_
