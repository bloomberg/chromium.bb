// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "sync/api/sync_change.h"
#include "url/gurl.h"

namespace syncer {
class SyncData;
}

namespace sync_pb {
class ExtensionSpecifics;
}

namespace extensions {

class Extension;

// A class that encapsulates the synced properties of an Extension.
class ExtensionSyncData {
 public:
  enum OptionalBoolean {
    BOOLEAN_UNSET,
    BOOLEAN_TRUE,
    BOOLEAN_FALSE
  };

  ExtensionSyncData();
  ExtensionSyncData(const Extension& extension,
                    bool enabled,
                    bool incognito_enabled,
                    bool remote_install,
                    OptionalBoolean all_urls_enabled);
  ~ExtensionSyncData();

  // For constructing an ExtensionSyncData from received sync data.
  // May return null if the sync data was invalid.
  static scoped_ptr<ExtensionSyncData> CreateFromSyncData(
      const syncer::SyncData& sync_data);
  static scoped_ptr<ExtensionSyncData> CreateFromSyncChange(
      const syncer::SyncChange& sync_change);

  // Retrieve sync data from this class.
  syncer::SyncData GetSyncData() const;
  syncer::SyncChange GetSyncChange(
      syncer::SyncChange::SyncChangeType change_type) const;

  // Convert an ExtensionSyncData back out to a sync structure.
  void PopulateExtensionSpecifics(sync_pb::ExtensionSpecifics* specifics) const;

  // Populate this class from sync inputs. Returns true if the input was valid.
  bool PopulateFromExtensionSpecifics(
      const sync_pb::ExtensionSpecifics& specifics);

  void set_uninstalled(bool uninstalled);

  const std::string& id() const { return id_; }

  // Version-independent properties (i.e., used even when the
  // version of the currently-installed extension doesn't match
  // |version|).
  bool uninstalled() const { return uninstalled_; }
  bool enabled() const { return enabled_; }
  bool incognito_enabled() const { return incognito_enabled_; }
  bool remote_install() const { return remote_install_; }
  OptionalBoolean all_urls_enabled() const { return all_urls_enabled_; }
  bool installed_by_custodian() const { return installed_by_custodian_; }

  // Version-dependent properties (i.e., should be used only when the
  // version of the currenty-installed extension matches |version|).
  const Version& version() const { return version_; }
  const GURL& update_url() const { return update_url_; }
  // Used only for debugging.
  const std::string& name() const { return name_; }

 private:
  // Populate this class from sync inputs.
  bool PopulateFromSyncData(const syncer::SyncData& sync_data);

  std::string id_;
  bool uninstalled_;
  bool enabled_;
  bool incognito_enabled_;
  bool remote_install_;
  OptionalBoolean all_urls_enabled_;
  bool installed_by_custodian_;
  Version version_;
  GURL update_url_;
  std::string name_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
