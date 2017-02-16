// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_ID_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_ID_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "components/sync/base/immutable.h"

namespace sync_pb {
class AttachmentIdProto;
}  // namespace sync_pb

namespace syncer {

// Uniquely identifies an attachment.
//
// Two attachments with equal (operator==) AttachmentIds are considered
// equivalent.
class AttachmentId {
 public:
  AttachmentId(const AttachmentId& other);
  ~AttachmentId();

  // Default copy and assignment are welcome.

  bool operator==(const AttachmentId& other) const;

  bool operator!=(const AttachmentId& other) const;

  // Needed for using AttachmentId as key in std::map.
  bool operator<(const AttachmentId& other) const;

  // Creates a unique id for an attachment.
  //
  // |size| is the attachment's size in bytes.
  //
  // |crc32c| is the attachment's crc32c.
  static AttachmentId Create(size_t size, uint32_t crc32c);

  // Creates an attachment id from an initialized proto.
  static AttachmentId CreateFromProto(const sync_pb::AttachmentIdProto& proto);

  const sync_pb::AttachmentIdProto& GetProto() const;

  // Returns the size (in bytes) the attachment.
  size_t GetSize() const;

  // Returns the crc32c the attachment.
  uint32_t GetCrc32c() const;

 private:
  // Necessary since we forward-declare sync_pb::AttachmentIdProto; see comments
  // in immutable.h.
  struct ImmutableAttachmentIdProtoTraits {
    using Wrapper = sync_pb::AttachmentIdProto*;
    static void InitializeWrapper(Wrapper* wrapper);
    static void DestroyWrapper(Wrapper* wrapper);
    static const sync_pb::AttachmentIdProto& Unwrap(const Wrapper& wrapper);
    static sync_pb::AttachmentIdProto* UnwrapMutable(Wrapper* wrapper);
    static void Swap(sync_pb::AttachmentIdProto* t1,
                     sync_pb::AttachmentIdProto* t2);
  };

  using ImmutableAttachmentIdProto =
      Immutable<sync_pb::AttachmentIdProto, ImmutableAttachmentIdProtoTraits>;

  ImmutableAttachmentIdProto proto_;

  explicit AttachmentId(sync_pb::AttachmentIdProto* proto);
};

// All public interfaces use AttachmentIdList. AttachmentIdSet is used in
// implementations of algorithms where set properties are needed.
using AttachmentIdList = std::vector<AttachmentId>;
using AttachmentIdSet = std::set<AttachmentId>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_ID_H_
