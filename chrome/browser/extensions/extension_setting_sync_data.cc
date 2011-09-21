// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_setting_sync_data.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/protocol/extension_setting_specifics.pb.h"

ExtensionSettingSyncData::ExtensionSettingSyncData(
    const SyncChange& sync_change) {
  Init(sync_change.change_type(), sync_change.sync_data());
}

ExtensionSettingSyncData::ExtensionSettingSyncData(
    const SyncData& sync_data) {
  Init(SyncChange::ACTION_INVALID, sync_data);
}

void ExtensionSettingSyncData::Init(
    SyncChange::SyncChangeType change_type, const SyncData& sync_data) {
  DCHECK(!internal_.get());
  sync_pb::ExtensionSettingSpecifics specifics =
      sync_data.GetSpecifics().GetExtension(sync_pb::extension_setting);
  Value* value =
      base::JSONReader().JsonToValue(specifics.value(), false, false);
  if (!value) {
    LOG(WARNING) << "Specifics for " << specifics.extension_id() << "/" <<
        specifics.key() << " had bad JSON for value: " << specifics.value();
    value = new DictionaryValue();
  }
  internal_ = new Internal(
      change_type,
      specifics.extension_id(),
      specifics.key(),
      value);
}

ExtensionSettingSyncData::ExtensionSettingSyncData(
    SyncChange::SyncChangeType change_type,
    const std::string& extension_id,
    const std::string& key,
    Value* value)
    : internal_(new Internal(change_type, extension_id, key, value)) {
  CHECK(value);
}

ExtensionSettingSyncData::~ExtensionSettingSyncData() {}

SyncChange::SyncChangeType ExtensionSettingSyncData::change_type() const {
  return internal_->change_type_;
}

const std::string& ExtensionSettingSyncData::extension_id() const {
  return internal_->extension_id_;
}

const std::string& ExtensionSettingSyncData::key() const {
  return internal_->key_;
}

const Value& ExtensionSettingSyncData::value() const {
  return *internal_->value_;
}

ExtensionSettingSyncData::Internal::Internal(
    SyncChange::SyncChangeType change_type,
    const std::string& extension_id,
    const std::string& key,
    Value* value)
    : change_type_(change_type),
      extension_id_(extension_id),
      key_(key),
      value_(value) {}

ExtensionSettingSyncData::Internal::~Internal() {}
