// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_id.h"

#include "base/logging.h"
#include "components/sync/base/attachment_id_proto.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

void AttachmentId::ImmutableAttachmentIdProtoTraits::InitializeWrapper(
    Wrapper* wrapper) {
  *wrapper = new sync_pb::AttachmentIdProto();
}

void AttachmentId::ImmutableAttachmentIdProtoTraits::DestroyWrapper(
    Wrapper* wrapper) {
  delete *wrapper;
}

const sync_pb::AttachmentIdProto&
AttachmentId::ImmutableAttachmentIdProtoTraits::Unwrap(const Wrapper& wrapper) {
  return *wrapper;
}

sync_pb::AttachmentIdProto*
AttachmentId::ImmutableAttachmentIdProtoTraits::UnwrapMutable(
    Wrapper* wrapper) {
  return *wrapper;
}

void AttachmentId::ImmutableAttachmentIdProtoTraits::Swap(
    sync_pb::AttachmentIdProto* t1,
    sync_pb::AttachmentIdProto* t2) {
  t1->Swap(t2);
}

AttachmentId::~AttachmentId() {}

bool AttachmentId::operator==(const AttachmentId& other) const {
  return proto_.Get().unique_id() == other.proto_.Get().unique_id();
}

bool AttachmentId::operator!=(const AttachmentId& other) const {
  return !operator==(other);
}

bool AttachmentId::operator<(const AttachmentId& other) const {
  return proto_.Get().unique_id() < other.proto_.Get().unique_id();
}

// Static.
AttachmentId AttachmentId::Create(size_t size, uint32_t crc32c) {
  sync_pb::AttachmentIdProto proto = CreateAttachmentIdProto(size, crc32c);
  return AttachmentId(&proto);
}

// Static.
AttachmentId AttachmentId::CreateFromProto(
    const sync_pb::AttachmentIdProto& proto) {
  sync_pb::AttachmentIdProto copy_of_proto(proto);
  return AttachmentId(&copy_of_proto);
}

const sync_pb::AttachmentIdProto& AttachmentId::GetProto() const {
  return proto_.Get();
}

AttachmentId::AttachmentId(sync_pb::AttachmentIdProto* proto) : proto_(proto) {}

AttachmentId::AttachmentId(const AttachmentId& other) = default;

size_t AttachmentId::GetSize() const {
  return proto_.Get().size_bytes();
}

uint32_t AttachmentId::GetCrc32c() const {
  return proto_.Get().crc32c();
}

}  // namespace syncer
