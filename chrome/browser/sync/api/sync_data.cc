// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/api/sync_data.h"

#include "chrome/browser/sync/protocol/sync.pb.h"

SyncData::SharedSyncEntity::SharedSyncEntity(
    sync_pb::SyncEntity* sync_entity)
    : sync_entity_(new sync_pb::SyncEntity()){
  sync_entity_->Swap(sync_entity);
}

const sync_pb::SyncEntity& SyncData::SharedSyncEntity::sync_entity() const {
  return *sync_entity_;
}

SyncData::SharedSyncEntity::~SharedSyncEntity() {}


SyncData::SyncData()
    : is_local_(true) {
}

SyncData::~SyncData() {
}

// Static.
SyncData SyncData::CreateLocalData(const std::string& sync_tag) {
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(sync_tag);
  SyncData a;
  a.shared_entity_ = new SharedSyncEntity(&entity);
  a.is_local_ = true;
  return a;
}

// Static.
SyncData SyncData::CreateLocalData(
    const std::string& sync_tag,
    const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(sync_tag);
  entity.mutable_specifics()->CopyFrom(specifics);
  SyncData a;
  a.shared_entity_ = new SharedSyncEntity(&entity);
  a.is_local_ = true;
  return a;
}

// Static.
SyncData SyncData::CreateRemoteData(const sync_pb::SyncEntity& entity) {
  // TODO(zea): eventually use this for building changes from the original sync
  // entities if possible.
  NOTIMPLEMENTED();
  return SyncData();
}

// Static.
SyncData SyncData::CreateRemoteData(
    const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->CopyFrom(specifics);
  SyncData a;
  a.shared_entity_ = new SharedSyncEntity(&entity);
  a.is_local_ = false;
  return a;
}

bool SyncData::IsValid() const {
  return (shared_entity_.get() != NULL);
}

const sync_pb::EntitySpecifics& SyncData::GetSpecifics() const {
  return shared_entity_->sync_entity().specifics();
}

syncable::ModelType SyncData::GetDataType() const {
  return syncable::GetModelTypeFromSpecifics(GetSpecifics());
}

const std::string& SyncData::GetTag() const {
  DCHECK(is_local_);
  return shared_entity_->sync_entity().client_defined_unique_tag();
}

bool SyncData::IsLocal() const {
  return is_local_;
}
