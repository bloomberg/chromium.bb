// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_SYNC_PROCESSOR_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_SYNC_PROCESSOR_H_
#pragma once

#include <set>
#include <string>

#include "chrome/browser/extensions/settings/setting_change.h"
#include "sync/api/sync_error.h"

class SyncChangeProcessor;

namespace extensions {

// A wrapper for a SyncChangeProcessor that deals specifically with the syncing
// of a single extension's settings. Handles:
//  - translating SettingChanges into calls into the Sync API.
//  - deciding whether to ADD/REMOVE/SET depending on the current state of
//    settings.
//  - rate limiting (inherently per-extension, which is what we want).
class SettingsSyncProcessor {
 public:
  SettingsSyncProcessor(const std::string& extension_id,
                        syncable::ModelType type,
                        SyncChangeProcessor* sync_processor);
  ~SettingsSyncProcessor();

  // Initializes this with the initial state of sync.
  void Init(const DictionaryValue& initial_state);

  // Sends |changes| to sync.
  SyncError SendChanges(const SettingChangeList& changes);

  // Informs this that |changes| have been receieved from sync. No action will
  // be taken, but this must be notified for internal bookkeeping.
  void NotifyChanges(const SettingChangeList& changes);

  syncable::ModelType type() { return type_; }

 private:
  // ID of the extension the changes are for.
  const std::string extension_id_;

  // Sync model type. Either EXTENSION_SETTING or APP_SETTING.
  const syncable::ModelType type_;

  // The sync processor used to send changes to sync.
  SyncChangeProcessor* const sync_processor_;

  // Whether Init() has been called.
  bool initialized_;

  // Keys of the settings that are currently being synced. Used to decide what
  // kind of action (ADD, UPDATE, REMOVE) to send to sync.
  std::set<std::string> synced_keys_;

  DISALLOW_COPY_AND_ASSIGN(SettingsSyncProcessor);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_SYNC_PROCESSOR_H_
