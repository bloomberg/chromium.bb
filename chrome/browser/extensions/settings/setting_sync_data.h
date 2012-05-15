// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTING_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTING_SYNC_DATA_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/api/sync_change.h"

class SyncData;
namespace sync_pb {
class ExtensionSettingSpecifics;
}

namespace extensions {

// Container for data interpreted from sync data/changes for an extension or
// app setting. Safe and efficient to copy.
class SettingSyncData {
 public:
  // Creates from a sync change.
  explicit SettingSyncData(const SyncChange& sync_change);

  // Creates from sync data. |change_type| will be ACTION_INVALID.
  explicit SettingSyncData(const SyncData& sync_data);

  // Creates explicitly.
  SettingSyncData(
      SyncChange::SyncChangeType change_type,
      const std::string& extension_id,
      const std::string& key,
      scoped_ptr<Value> value);

  ~SettingSyncData();

  // Returns the type of the sync change; may be ACTION_INVALID.
  SyncChange::SyncChangeType change_type() const;

  // Returns the extension id the setting is for.
  const std::string& extension_id() const;

  // Returns the settings key.
  const std::string& key() const;

  // Returns the value of the setting.
  const Value& value() const;

 private:
  // Ref-counted container for the data.
  // TODO(kalman): Use browser_sync::Immutable<Internal>.
  class Internal : public base::RefCountedThreadSafe<Internal> {
   public:
    Internal(
      SyncChange::SyncChangeType change_type,
      const std::string& extension_id,
      const std::string& key,
      scoped_ptr<Value> value);

    SyncChange::SyncChangeType change_type_;
    std::string extension_id_;
    std::string key_;
    scoped_ptr<Value> value_;

   private:
    friend class base::RefCountedThreadSafe<Internal>;
    ~Internal();
  };

  // Initializes internal_ from sync data for an extension or app setting.
  void Init(SyncChange::SyncChangeType change_type, const SyncData& sync_data);

  // Initializes internal_ from extension specifics.
  void InitFromExtensionSettingSpecifics(
      SyncChange::SyncChangeType change_type,
      const sync_pb::ExtensionSettingSpecifics& specifics);

  scoped_refptr<Internal> internal_;
};

typedef std::vector<SettingSyncData> SettingSyncDataList;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTING_SYNC_DATA_H_
