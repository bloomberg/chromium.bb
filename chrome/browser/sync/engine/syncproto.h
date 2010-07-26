// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wrappers to help us work with ids and protobuffers.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCPROTO_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCPROTO_H_
#pragma once

#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable_id.h"

namespace browser_sync {

template<class Base>
class IdWrapper : public Base {
 public:
  IdWrapper() {}
  explicit IdWrapper(const Base& other) : Base(other) {
  }
  syncable::Id id() const {
    return syncable::Id::CreateFromServerId(Base::id_string());
  }
  void set_id(const syncable::Id& id) {
    Base::set_id_string(id.GetServerId());
  }
};

// These wrapper classes contain no data, so their super classes can be cast to
// them directly.
class SyncEntity : public IdWrapper<sync_pb::SyncEntity> {
 public:
  SyncEntity() {}
  explicit SyncEntity(const sync_pb::SyncEntity& other)
      : IdWrapper<sync_pb::SyncEntity>(other) {
  }

  void set_parent_id(const syncable::Id& id) {
    set_parent_id_string(id.GetServerId());
  }
  syncable::Id parent_id() const {
    return syncable::Id::CreateFromServerId(parent_id_string());
  }
  void set_old_parent_id(const syncable::Id& id) {
    IdWrapper<sync_pb::SyncEntity>::set_old_parent_id(
      id.GetServerId());
  }
  syncable::Id old_parent_id() const {
    return syncable::Id::CreateFromServerId(
      sync_pb::SyncEntity::old_parent_id());
  }
  // Binary predicate helper to determine whether an Entity represents a folder
  // or non-folder object. Use this instead of checking these properties
  // directly, because the addition of bookmarks to the protobuf schema
  // makes the check slightly more tricky.
  bool IsFolder() const {
    return ((has_folder() && folder()) ||
            (has_bookmarkdata() && bookmarkdata().bookmark_folder()));
  }

  syncable::ModelType GetModelType() const {
    return syncable::GetModelType(*this);
  }
};

class CommitResponse_EntryResponse
    : public IdWrapper<sync_pb::CommitResponse_EntryResponse> {
};

class ClientToServerMessage : public sync_pb::ClientToServerMessage {
 public:
  ClientToServerMessage() {
    set_protocol_version(protocol_version());
  }
};

typedef sync_pb::CommitMessage CommitMessage;
typedef sync_pb::ClientToServerResponse ClientToServerResponse;
typedef sync_pb::CommitResponse CommitResponse;
typedef sync_pb::GetUpdatesResponse GetUpdatesResponse;
typedef sync_pb::GetUpdatesMessage GetUpdatesMessage;

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCPROTO_H_
