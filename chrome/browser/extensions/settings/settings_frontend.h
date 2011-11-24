// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/sync/api/syncable_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class SettingsStorage;

// The component of extension settings which runs on the UI thread, as opposed
// to SettingsBackend which lives on the FILE thread.
// All public methods must be called on the UI thread.
class SettingsFrontend {
 public:
  // Creates with the default factory.  Ownership of |profile| not taken.
  static SettingsFrontend* Create(Profile* profile);

  static SettingsFrontend* Create(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      // Owership NOT taken.
      Profile* profile);

  virtual ~SettingsFrontend();

  typedef base::Callback<void(SyncableService*)> SyncableServiceCallback;
  typedef base::Callback<void(SettingsStorage*)> StorageCallback;

  // Runs |callback| on the FILE thread with the SyncableService for
  // |model_type|, either SETTINGS or APP_SETTINGS.
  void RunWithSyncableService(
      syncable::ModelType model_type, const SyncableServiceCallback& callback);

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

  // The (non-incognito) Profile this Frontend belongs to.
  Profile* const profile_;

  // List of observers to settings changes.
  scoped_refptr<SettingsObserverList> observers_;

  // Observer for |profile_|.
  scoped_ptr<SettingsObserver> profile_observer_;

  // Ref-counted container for each SettingsBackend object.  There are 4: an
  // apps and an extensions Backend for the LOCAL namespace, and likewise for
  // the SYNC namespace.  They only differ in what directory the database for
  // each exists in (and the Backends in the SYNC namespace happen to be
  // returned from RunWithSyncableService).
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
