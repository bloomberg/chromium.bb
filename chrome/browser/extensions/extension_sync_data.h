// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#pragma once

#include <string>

#include "base/version.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

class SyncData;
namespace sync_pb {
class AppSpecifics;
class ExtensionSpecifics;
}

// A class that encapsulates the synced properties of an Extension.
class ExtensionSyncData {
 public:
  ExtensionSyncData();
  explicit ExtensionSyncData(const SyncData& sync_data);
  explicit ExtensionSyncData(const SyncChange& sync_change);
  ExtensionSyncData(const Extension& extension,
                    bool enabled,
                    bool incognito_enabled,
                    const std::string& notifications_client_id,
                    bool notifications_disabled);
  ~ExtensionSyncData();

  // Convert an ExtensionSyncData back out to a sync structure.
  void PopulateSyncSpecifics(sync_pb::ExtensionSpecifics* specifics) const;
  void PopulateAppSpecifics(sync_pb::AppSpecifics* specifics) const;
  SyncData GetSyncData() const;
  SyncChange GetSyncChange(SyncChange::SyncChangeType change_type) const;

  const std::string& id() const { return id_; }

  // Version-independent properties (i.e., used even when the
  // version of the currently-installed extension doesn't match
  // |version|).
  bool uninstalled() const { return uninstalled_; }
  bool enabled() const { return enabled_; }
  bool incognito_enabled() const { return incognito_enabled_; }
  Extension::SyncType type() const { return type_; }

  // Version-dependent properties (i.e., should be used only when the
  // version of the currenty-installed extension matches |version|).
  const Version& version() const { return version_; }
  const GURL& update_url() const { return update_url_; }
  // Used only for debugging.
  const std::string& name() const { return name_; }

  const std::string& notifications_client_id() const {
    return notifications_client_id_;
  }

  bool notifications_disabled() const {
    return notifications_disabled_;
  }

 private:
  void PopulateFromExtensionSpecifics(
      const sync_pb::ExtensionSpecifics& specifics);
  void PopulateFromSyncData(const SyncData& sync_data);

  std::string id_;
  bool uninstalled_;
  bool enabled_;
  bool incognito_enabled_;
  Extension::SyncType type_;
  Version version_;
  GURL update_url_;
  std::string name_;
  std::string notifications_client_id_;
  bool notifications_disabled_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
