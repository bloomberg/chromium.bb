// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "extensions/common/constants.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "third_party/skia/include/core/SkColor.h"

namespace syncer {
class SyncData;
}

namespace sync_pb {
class AppSpecifics;
}

namespace extensions {

class Extension;
class ExtensionSyncData;

// A class that encapsulates the synced properties of an Application.
class AppSyncData {
 public:
  AppSyncData();
  AppSyncData(const Extension& extension,
              bool enabled,
              bool incognito_enabled,
              bool remote_install,
              ExtensionSyncData::OptionalBoolean all_urls_enabled,
              const syncer::StringOrdinal& app_launch_ordinal,
              const syncer::StringOrdinal& page_ordinal,
              extensions::LaunchType launch_type);
  ~AppSyncData();

  // For constructing an AppSyncData from received sync data.
  // May return null if the sync data was invalid.
  static scoped_ptr<AppSyncData> CreateFromSyncData(
      const syncer::SyncData& sync_data);
  static scoped_ptr<AppSyncData> CreateFromSyncChange(
      const syncer::SyncChange& sync_change);

  // Retrive sync data from this class.
  syncer::SyncData GetSyncData() const;
  syncer::SyncChange GetSyncChange(
      syncer::SyncChange::SyncChangeType change_type) const;

  const std::string& id() const { return extension_sync_data_.id(); }

  bool uninstalled() const { return extension_sync_data_.uninstalled(); }

  // These ordinals aren't necessarily valid. Some applications don't have
  // valid ordinals because they don't appear on the new tab page.
  const syncer::StringOrdinal& app_launch_ordinal() const {
    return app_launch_ordinal_;
  }
  const syncer::StringOrdinal& page_ordinal() const { return page_ordinal_; }

  const ExtensionSyncData& extension_sync_data() const {
    return extension_sync_data_;
  }

  extensions::LaunchType launch_type() const {
    return launch_type_;
  }

  const std::string& bookmark_app_url() const {
    return bookmark_app_url_;
  }

  const std::string& bookmark_app_description() const {
    return bookmark_app_description_;
  }

  const std::string& bookmark_app_icon_color() const {
    return bookmark_app_icon_color_;
  }

 private:
  // Convert an AppSyncData back out to a sync structure.
  void PopulateAppSpecifics(sync_pb::AppSpecifics* specifics) const;

  // Populate this class from sync inputs. Return true if the input
  // was valid.
  bool PopulateFromAppSpecifics(const sync_pb::AppSpecifics& specifics);
  bool PopulateFromSyncData(const syncer::SyncData& sync_data);

  ExtensionSyncData extension_sync_data_;
  syncer::StringOrdinal app_launch_ordinal_;
  syncer::StringOrdinal page_ordinal_;
  extensions::LaunchType launch_type_;
  std::string bookmark_app_url_;
  std::string bookmark_app_description_;
  std::string bookmark_app_icon_color_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_SYNC_DATA_H_
