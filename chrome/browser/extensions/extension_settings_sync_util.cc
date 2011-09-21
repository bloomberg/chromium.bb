// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_sync_util.h"

#include "base/values.h"
#include "base/json/json_writer.h"
#include "chrome/browser/sync/protocol/extension_setting_specifics.pb.h"

namespace extension_settings_sync_util {

SyncData CreateData(
    const std::string& extension_id,
    const std::string& key,
    const Value& value) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSettingSpecifics* setting_specifics =
      specifics.MutableExtension(sync_pb::extension_setting);
  setting_specifics->set_extension_id(extension_id);
  setting_specifics->set_key(key);
  std::string value_as_json;
  base::JSONWriter::Write(&value, false, &value_as_json);
  setting_specifics->set_value(value_as_json);
  return SyncData::CreateLocalData(extension_id + "/" + key, key, specifics);
}

SyncChange CreateAdd(
    const std::string& extension_id,
    const std::string& key,
    const Value& value) {
  return SyncChange(
      SyncChange::ACTION_ADD, CreateData(extension_id, key, value));
}

SyncChange CreateUpdate(
    const std::string& extension_id,
    const std::string& key,
    const Value& value) {
  return SyncChange(
      SyncChange::ACTION_UPDATE, CreateData(extension_id, key, value));
}

SyncChange CreateDelete(
    const std::string& extension_id, const std::string& key) {
  DictionaryValue no_value;
  return SyncChange(
      SyncChange::ACTION_DELETE, CreateData(extension_id, key, no_value));
}

}  // namespace extension_settings_sync_util
