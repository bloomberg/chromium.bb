// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_FRONTEND_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_FRONTEND_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/extensions/settings/settings_leveldb_storage.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_observer.h"
#include "chrome/browser/extensions/settings/settings_storage_quota_enforcer.h"
#include "sync/api/syncable_service.h"

class Profile;

namespace extensions {

class SettingsStorage;

// The component of extension settings which runs on the UI thread, as opposed
// to SettingsBackend which lives on the FILE thread.
// All public methods, must be called on the UI thread, with the exception of
// GetBackendForSync(), which must be called on the FILE thread.
class SettingsFrontend {
 public:
  // Creates with the default factory. Ownership of |profile| not taken.
  static SettingsFrontend* Create(Profile* profile);

  // Creates with a specific factory |storage_factory| (presumably for tests).
  // Ownership of |profile| not taken.
  static SettingsFrontend* Create(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      // Owership NOT taken.
      Profile* profile);

  virtual ~SettingsFrontend();

  typedef base::Callback<void(SyncableService*)> SyncableServiceCallback;
  typedef base::Callback<void(SettingsStorage*)> StorageCallback;

  // Must only be called from the FILE thread. |type| should be either
  // APP_SETTINGS or EXTENSION_SETTINGS.
  SyncableService* GetBackendForSync(syncable::ModelType type) const;

  // Runs |callback| on the FILE thread with the storage area for
  // |extension_id|.  If there is no extension with that ID, the storage area
  // will be NULL.
  void RunWithStorage(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const StorageCallback& callback);

  // Deletes the settings for an extension (on the FILE thread).
  void DeleteStorageSoon(const std::string& extension_id);

  // Gets the thread-safe observer list.
  scoped_refptr<SettingsObserverList> GetObservers();

 private:
  SettingsFrontend(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      // Ownership NOT taken.
      Profile* profile);

  // The quota limit configurations for the local and sync areas, taken out of
  // the schema in chrome/common/extensions/api/storage.json.
  const SettingsStorageQuotaEnforcer::Limits local_quota_limit_;
  const SettingsStorageQuotaEnforcer::Limits sync_quota_limit_;

  // The (non-incognito) Profile this Frontend belongs to.
  Profile* const profile_;

  // List of observers to settings changes.
  scoped_refptr<SettingsObserverList> observers_;

  // Observer for |profile_|.
  scoped_ptr<SettingsObserver> profile_observer_;

  // Ref-counted container for each SettingsBackend object.  There are 4: an
  // apps and an extensions Backend for the LOCAL namespace, and likewise for
  // the SYNC namespace.  They only differ in what directory the database for
  // each exists in.
  class BackendWrapper;
  struct BackendWrappers {
    BackendWrappers();
    ~BackendWrappers();
    scoped_refptr<BackendWrapper> app;
    scoped_refptr<BackendWrapper> extension;
  };
  std::map<settings_namespace::Namespace, BackendWrappers> backends_;

  DISALLOW_COPY_AND_ASSIGN(SettingsFrontend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_FRONTEND_H_
