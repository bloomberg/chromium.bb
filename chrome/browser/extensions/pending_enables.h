// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_ENABLES_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_ENABLES_H_

#include <set>

// Included for syncer::ModelType, which is in
// sync/internal_api/public/base/model_type.h but that is disallowed by
// chrome/browser/DEPS.
#include "sync/api/syncable_service.h"

class ExtensionService;
class ProfileSyncService;

namespace sync_driver {
class SyncPrefs;
}

namespace extensions {

class SyncBundle;

// A pending enable is when an app or extension is enabled locally before sync
// has started. We track these to prevent sync data arriving and clobbering
// local state, and also to ensure that these early enables get synced to the
// server when sync does start.
class PendingEnables {
 public:
  PendingEnables(scoped_ptr<sync_driver::SyncPrefs> sync_prefs,
                 SyncBundle* sync_bundle,
                 syncer::ModelType enable_type);
  ~PendingEnables();

  // Called when an extension is enabled / disabled locally.
  // These will check the sync state and figure out whether the change
  // needs to be remembered for syncing when syncing starts.
  void OnExtensionEnabled(const std::string& extension_id);
  void OnExtensionDisabled(const std::string& extension_id);

  // Called when |sync_bundle_| is ready to accept sync changes.
  // Uses |service| to look up extensions from extension ids.
  void OnSyncStarted(ExtensionService* service);

  // Whether |extension_id| has a pending enable.
  bool Contains(const std::string& extension_id) const;

 private:
  bool IsSyncEnabled();
  bool IsWaitingForSync();

  scoped_ptr<sync_driver::SyncPrefs> sync_prefs_;
  SyncBundle* sync_bundle_;
  syncer::ModelType enable_type_;
  std::set<std::string> ids_;

  bool is_sync_enabled_for_test_;

  DISALLOW_COPY_AND_ASSIGN(PendingEnables);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_ENABLES_H_
