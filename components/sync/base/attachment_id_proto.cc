// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/attachment_id_proto.h"

#include <string>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace syncer {

sync_pb::AttachmentIdProto CreateAttachmentIdProto(size_t size,
                                                   uint32_t crc32c) {
  sync_pb::AttachmentIdProto proto;
  std::string guid = base::ToLowerASCII(base::GenerateGUID());
  DCHECK(!guid.empty());
  // Requirements are that this id must be a unique RFC4122 UUID, formatted in
  // lower case.
  proto.set_unique_id(guid);
  proto.set_size_bytes(size);
  proto.set_crc32c(crc32c);
  return proto;
}

sync_pb::AttachmentMetadata CreateAttachmentMetadata(
    const google::protobuf::RepeatedPtrField<sync_pb::AttachmentIdProto>& ids) {
  sync_pb::AttachmentMetadata result;
  for (int i = 0; i < ids.size(); ++i) {
    sync_pb::AttachmentMetadataRecord* record = result.add_record();
    *record->mutable_id() = ids.Get(i);
    record->set_is_on_server(true);
  }
  return result;
}

}  // namespace syncer
