// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/setting_sync_data.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/app_setting_specifics.pb.h"
#include "sync/protocol/extension_setting_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace extensions {

SettingSyncData::SettingSyncData(
    const SyncChange& sync_change) {
  Init(sync_change.change_type(), sync_change.sync_data());
}

SettingSyncData::SettingSyncData(
    const SyncData& sync_data) {
  Init(SyncChange::ACTION_INVALID, sync_data);
}

void SettingSyncData::Init(
    SyncChange::SyncChangeType change_type, const SyncData& sync_data) {
  DCHECK(!internal_.get());
  sync_pb::EntitySpecifics specifics = sync_data.GetSpecifics();
  // The data must only be either extension or app specfics.
  DCHECK_NE(specifics.has_extension_setting(),
            specifics.has_app_setting());
  if (specifics.has_extension_setting()) {
    InitFromExtensionSettingSpecifics(
        change_type,
        specifics.extension_setting());
  } else if (specifics.has_app_setting()) {
    InitFromExtensionSettingSpecifics(
        change_type,
        specifics.app_setting().extension_setting());
  }
}

void SettingSyncData::InitFromExtensionSettingSpecifics(
    SyncChange::SyncChangeType change_type,
    const sync_pb::ExtensionSettingSpecifics& specifics) {
  DCHECK(!internal_.get());
  scoped_ptr<Value> value(
      base::JSONReader::Read(specifics.value()));
  if (!value.get()) {
    LOG(WARNING) << "Specifics for " << specifics.extension_id() << "/" <<
        specifics.key() << " had bad JSON for value: " << specifics.value();
    value.reset(new DictionaryValue());
  }
  internal_ = new Internal(
      change_type,
      specifics.extension_id(),
      specifics.key(),
      value.Pass());
}

SettingSyncData::SettingSyncData(
    SyncChange::SyncChangeType change_type,
    const std::string& extension_id,
    const std::string& key,
    scoped_ptr<Value> value)
    : internal_(new Internal(change_type, extension_id, key, value.Pass())) {}

SettingSyncData::~SettingSyncData() {}

SyncChange::SyncChangeType SettingSyncData::change_type() const {
  return internal_->change_type_;
}

const std::string& SettingSyncData::extension_id() const {
  return internal_->extension_id_;
}

const std::string& SettingSyncData::key() const {
  return internal_->key_;
}

const Value& SettingSyncData::value() const {
  return *internal_->value_;
}

SettingSyncData::Internal::Internal(
    SyncChange::SyncChangeType change_type,
    const std::string& extension_id,
    const std::string& key,
    scoped_ptr<Value> value)
    : change_type_(change_type),
      extension_id_(extension_id),
      key_(key),
      value_(value.Pass()) {
  DCHECK(value_.get());
}

SettingSyncData::Internal::~Internal() {}

}  // namespace extensions
