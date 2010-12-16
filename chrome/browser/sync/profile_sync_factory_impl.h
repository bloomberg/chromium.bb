// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_factory.h"

class CommandLine;
class Profile;

class ProfileSyncFactoryImpl : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryImpl(Profile* profile, CommandLine* command_line);
  virtual ~ProfileSyncFactoryImpl() {}

  // ProfileSyncFactory interface.
  virtual ProfileSyncService* CreateProfileSyncService(
      const std::string& cros_user);

  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap& controllers);

  virtual SyncComponents CreateAppSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateAutofillSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler);

  virtual SyncComponents CreateAutofillProfileSyncComponents(
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

  virtual SyncComponents CreateSessionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler);

 private:
  Profile* profile_;
  CommandLine* command_line_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_IMPL_H__
