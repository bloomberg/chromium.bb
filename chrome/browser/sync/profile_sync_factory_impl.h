// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_factory.h"

class CommandLine;
class Profile;

namespace chrome_common_net {
class NetworkChangeNotifierThread;
}

class ProfileSyncFactoryImpl : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryImpl(
      Profile* profile,
      chrome_common_net::NetworkChangeNotifierThread*
          network_change_notifier_thread,
      CommandLine* command_line);
  virtual ~ProfileSyncFactoryImpl() {}

  // ProfileSyncFactory interface.
  virtual ProfileSyncService* CreateProfileSyncService();

  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap& controllers);

  virtual SyncComponents CreateAutofillSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateExtensionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreatePasswordSyncComponents(
      ProfileSyncService* profile_sync_service,
      PasswordStore* password_store,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreatePreferenceSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateThemeSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::UnrecoverableErrorHandler* error_handler);

 private:
  Profile* profile_;
  chrome_common_net::NetworkChangeNotifierThread*
      network_change_notifier_thread_;
  CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
