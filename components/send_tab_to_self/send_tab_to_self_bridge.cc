// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include "base/optional.h"

#include "components/sync/model/metadata_change_list.h"

namespace send_tab_to_self {

SendTabToSelfBridge::SendTabToSelfBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {
  NOTIMPLEMENTED();
}

SendTabToSelfBridge::~SendTabToSelfBridge() {
  NOTIMPLEMENTED();
}

std::unique_ptr<syncer::MetadataChangeList>
SendTabToSelfBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::Optional<syncer::ModelError>();
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return base::Optional<syncer::ModelError>();
}

void SendTabToSelfBridge::GetData(StorageKeyList storage_keys,
                                  DataCallback callback) {
  NOTIMPLEMENTED();
}

void SendTabToSelfBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string SendTabToSelfBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

std::string SendTabToSelfBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return std::string();
}

bool SendTabToSelfBridge::loaded() const {
  NOTIMPLEMENTED();
  return false;
}

const std::vector<GURL> SendTabToSelfBridge::Keys() const {
  NOTIMPLEMENTED();
  return std::vector<GURL>();
}

bool SendTabToSelfBridge::DeleteAllEntries() {
  NOTIMPLEMENTED();
  return false;
}

const SendTabToSelfEntry* SendTabToSelfBridge::GetEntryByURL(
    const GURL& gurl) const {
  NOTIMPLEMENTED();
  return nullptr;
}

const SendTabToSelfEntry* SendTabToSelfBridge::GetMostRecentEntry() const {
  NOTIMPLEMENTED();
  return nullptr;
}

const SendTabToSelfEntry* SendTabToSelfBridge::AddEntry(
    const GURL& url,
    const std::string& title) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace send_tab_to_self
