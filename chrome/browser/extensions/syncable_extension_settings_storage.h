// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNCABLE_EXTENSION_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_SYNCABLE_EXTENSION_SETTINGS_STORAGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_settings_observer.h"
#include "chrome/browser/extensions/extension_settings_storage.h"
#include "chrome/browser/extensions/extension_setting_sync_data.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/api/sync_change.h"

// Decorates an ExtensionSettingsStorage with sync behaviour.
class SyncableExtensionSettingsStorage : public ExtensionSettingsStorage {
 public:
  explicit SyncableExtensionSettingsStorage(
      const scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> >&
          observers,
      const std::string& extension_id,
      // Ownership taken.
      ExtensionSettingsStorage* delegate);

  virtual ~SyncableExtensionSettingsStorage();

  // ExtensionSettingsStorage implementation.
  virtual ReadResult Get(const std::string& key) OVERRIDE;
  virtual ReadResult Get(const std::vector<std::string>& keys) OVERRIDE;
  virtual ReadResult Get() OVERRIDE;
  virtual WriteResult Set(const std::string& key, const Value& value) OVERRIDE;
  virtual WriteResult Set(const DictionaryValue& settings) OVERRIDE;
  virtual WriteResult Remove(const std::string& key) OVERRIDE;
  virtual WriteResult Remove(const std::vector<std::string>& keys) OVERRIDE;
  virtual WriteResult Clear() OVERRIDE;

  // Sync-related methods, analogous to those on SyncableService (handled by
  // ExtensionSettings).
  SyncError StartSyncing(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncable::ModelType type,
      const DictionaryValue& sync_state,
      // Must NOT be NULL. Ownership NOT taken.
      SyncChangeProcessor* sync_processor);
  void StopSyncing();
  std::vector<SyncError> ProcessSyncChanges(
      const ExtensionSettingSyncDataList& sync_changes);

 private:
  // Propagates some changes to sync by sending an ADD/UPDATE/DELETE depending
  // on the change and the current state of sync.
  void SendChangesToSync(const ExtensionSettingChangeList& changes);

  // Sends all local settings to sync (synced settings assumed to be empty).
  SyncError SendLocalSettingsToSync(
      const DictionaryValue& settings);

  // Overwrites local state with sync state.
  SyncError OverwriteLocalSettingsWithSync(
      const DictionaryValue& sync_state, const DictionaryValue& settings);

  // Called when an Add/Update/Remove comes from sync.  Ownership of Value*s
  // are taken.
  SyncError OnSyncAdd(
      const std::string& key,
      Value* new_value,
      ExtensionSettingChangeList* changes);
  SyncError OnSyncUpdate(
      const std::string& key,
      Value* old_value,
      Value* new_value,
      ExtensionSettingChangeList* changes);
  SyncError OnSyncDelete(
      const std::string& key,
      Value* old_value,
      ExtensionSettingChangeList* changes);

  // List of observers to settings changes.
  const scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> >
      observers_;

  // Id of the extension these settings are for.
  std::string const extension_id_;

  // Storage area to sync.
  const scoped_ptr<ExtensionSettingsStorage> delegate_;

  // Sync model type.  Either EXTENSION_SETTINGS or APP_SETTINGS while sync is
  // enabled (between calls to Start/StopSyncing), or UNSPECIFIED while not.
  syncable::ModelType sync_type_;

  // Sync processor.  Non-NULL while sync is enabled (between calls to
  // StartSyncing and StopSyncing).
  SyncChangeProcessor* sync_processor_;

  // Keys of the settings that are currently being synced.  Only used to decide
  // whether an ACTION_ADD or ACTION_UPDATE should be sent to sync on a Set().
  std::set<std::string> synced_keys_;

  DISALLOW_COPY_AND_ASSIGN(SyncableExtensionSettingsStorage);
};

#endif  // CHROME_BROWSER_EXTENSIONS_SYNCABLE_EXTENSION_SETTINGS_STORAGE_H_
