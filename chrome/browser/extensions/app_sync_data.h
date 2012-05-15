// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_
#pragma once

#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/common/string_ordinal.h"
#include "sync/api/sync_change.h"

class Extension;
class SyncData;
namespace sync_pb {
class AppSpecifics;
}

namespace extensions {

class ExtensionSyncData;

// A class that encapsulates the synced properties of an Application.
class AppSyncData {
 public:
  AppSyncData();
  explicit AppSyncData(const SyncData& sync_data);
  explicit AppSyncData(const SyncChange& sync_change);
  AppSyncData(const Extension& extension,
              bool enabled,
              bool incognito_enabled,
              const std::string& notifications_client_id,
              bool notifications_disabled,
              const StringOrdinal& app_launch_ordinal,
              const StringOrdinal& page_ordinal);
  ~AppSyncData();

  // Retrive sync data from this class.
  SyncData GetSyncData() const;
  SyncChange GetSyncChange(SyncChange::SyncChangeType change_type) const;

  const std::string& id() const { return extension_sync_data_.id(); }

  bool uninstalled() const { return extension_sync_data_.uninstalled(); }

  const std::string& notifications_client_id() const {
    return notifications_client_id_;
  }

  bool notifications_disabled() const {
    return notifications_disabled_;
  }

  // These ordinals aren't necessarily valid. Some applications don't have
  // valid ordinals because they don't appear on the new tab page.
  const StringOrdinal& app_launch_ordinal() const {
    return app_launch_ordinal_;
  }
  const StringOrdinal& page_ordinal() const { return page_ordinal_; }

  const ExtensionSyncData& extension_sync_data() const {
    return extension_sync_data_;
  }

 private:
  // Convert an AppSyncData back out to a sync structure.
  void PopulateAppSpecifics(sync_pb::AppSpecifics* specifics) const;

  // Populate this class from sync inputs.
  void PopulateFromAppSpecifics(
      const sync_pb::AppSpecifics& specifics);
  void PopulateFromSyncData(const SyncData& sync_data);

  ExtensionSyncData extension_sync_data_;
  std::string notifications_client_id_;
  bool notifications_disabled_;
  StringOrdinal app_launch_ordinal_;
  StringOrdinal page_ordinal_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_
