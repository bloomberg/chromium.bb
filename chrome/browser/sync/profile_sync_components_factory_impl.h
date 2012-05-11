// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

class CommandLine;
class ExtensionSystem;
class Profile;

class ProfileSyncComponentsFactoryImpl : public ProfileSyncComponentsFactory {
 public:
  ProfileSyncComponentsFactoryImpl(Profile* profile,
                                   CommandLine* command_line);
  virtual ~ProfileSyncComponentsFactoryImpl() {}

  virtual void RegisterDataTypes(ProfileSyncService* pss) OVERRIDE;

  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap* controllers) OVERRIDE;

  virtual browser_sync::GenericChangeProcessor* CreateGenericChangeProcessor(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler,
      const base::WeakPtr<SyncableService>& local_service) OVERRIDE;

  virtual browser_sync::SharedChangeProcessor*
      CreateSharedChangeProcessor() OVERRIDE;

  virtual base::WeakPtr<SyncableService> GetSyncableServiceForType(
      syncable::ModelType type) OVERRIDE;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler) OVERRIDE;
  virtual SyncComponents CreatePasswordSyncComponents(
      ProfileSyncService* profile_sync_service,
      PasswordStore* password_store,
      browser_sync::DataTypeErrorHandler* error_handler) OVERRIDE;
#if defined(ENABLE_THEMES)
  virtual SyncComponents CreateThemeSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler) OVERRIDE;
#endif
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::DataTypeErrorHandler* error_handler) OVERRIDE;
  virtual SyncComponents CreateSessionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler) OVERRIDE;

 private:
  Profile* profile_;
  CommandLine* command_line_;
  // Set on the UI thread (since ExtensionSystemFactory is non-threadsafe);
  // accessed on both the UI and FILE threads in GetSyncableServiceForType.
  ExtensionSystem* extension_system_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncComponentsFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
