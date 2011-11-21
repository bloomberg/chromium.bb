// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_factory.h"

class CommandLine;
class Profile;

class ProfileSyncFactoryImpl : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryImpl(Profile* profile, CommandLine* command_line);
  virtual ~ProfileSyncFactoryImpl() {}

  // ProfileSyncFactory interface.
  virtual ProfileSyncService* CreateProfileSyncService(
      const std::string& cros_user) OVERRIDE;

  virtual void RegisterDataTypes(ProfileSyncService* pss) OVERRIDE;

  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap* controllers) OVERRIDE;

  virtual browser_sync::GenericChangeProcessor* CreateGenericChangeProcessor(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler,
      const base::WeakPtr<SyncableService>& local_service) OVERRIDE;

  virtual browser_sync::SharedChangeProcessor*
      CreateSharedChangeProcessor() OVERRIDE;

  virtual SyncComponents CreateAppSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual base::WeakPtr<SyncableService> GetAutofillProfileSyncableService(
      WebDataService* web_data_service) const OVERRIDE;

  virtual base::WeakPtr<SyncableService> GetAutocompleteSyncableService(
      WebDataService* web_data_service) const OVERRIDE;

  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateExtensionOrAppSettingSyncComponents(
      // Either EXTENSION_SETTING or APP_SETTING.
      syncable::ModelType type,
      SyncableService* settings_service,
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateExtensionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreatePasswordSyncComponents(
      ProfileSyncService* profile_sync_service,
      PasswordStore* password_store,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreatePreferenceSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateThemeSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateSessionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateSearchEngineSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

  virtual SyncComponents CreateAppNotificationSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) OVERRIDE;

 private:
  Profile* profile_;
  CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
