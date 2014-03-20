// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pending_enables.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sync_bundle.h"
#include "components/sync_driver/sync_prefs.h"

namespace extensions {

PendingEnables::PendingEnables(scoped_ptr<sync_driver::SyncPrefs> sync_prefs,
                               SyncBundle* sync_bundle,
                               syncer::ModelType enable_type)
    : sync_prefs_(sync_prefs.Pass()),
      sync_bundle_(sync_bundle),
      enable_type_(enable_type),
      is_sync_enabled_for_test_(false) {}

PendingEnables::~PendingEnables() {
}

void PendingEnables::OnExtensionEnabled(const std::string& extension_id) {
  if (IsWaitingForSync())
    ids_.insert(extension_id);
}

void PendingEnables::OnExtensionDisabled(const std::string& extension_id) {
  if (IsWaitingForSync())
    ids_.erase(extension_id);
}

void PendingEnables::OnSyncStarted(ExtensionService* service) {
  for (std::set<std::string>::const_iterator it = ids_.begin();
       it != ids_.end(); ++it) {
    const Extension* extension = service->GetExtensionById(*it, true);
    if (extension)
      sync_bundle_->SyncChangeIfNeeded(*extension);
  }
  ids_.clear();
}

bool PendingEnables::Contains(const std::string& extension_id) const {
  return ids_.find(extension_id) != ids_.end();
}

bool PendingEnables::IsSyncEnabled() {
  if (is_sync_enabled_for_test_)
    return true;
  return sync_prefs_ &&
      sync_prefs_->HasSyncSetupCompleted() &&
      sync_prefs_->GetPreferredDataTypes(syncer::ModelTypeSet(enable_type_))
          .Has(enable_type_);
}

bool PendingEnables::IsWaitingForSync() {
  return IsSyncEnabled() && !sync_bundle_->IsSyncing();
}

}  // namespace extensions

