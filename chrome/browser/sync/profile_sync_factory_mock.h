// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
}

class ProfileSyncFactoryMock : public ProfileSyncFactory {
 public:
  ProfileSyncFactoryMock();
  ProfileSyncFactoryMock(
      browser_sync::AssociatorInterface* bookmark_model_associator,
      browser_sync::ChangeProcessor* bookmark_change_processor);
  virtual ~ProfileSyncFactoryMock();

  MOCK_METHOD1(CreateProfileSyncService,
               ProfileSyncService*(const std::string&));
  MOCK_METHOD2(CreateDataTypeManager,
               browser_sync::DataTypeManager*(
                   browser_sync::SyncBackendHost*,
                   const browser_sync::DataTypeController::TypeMap&));
  MOCK_METHOD2(CreateAppSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD4(CreateAutofillSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   WebDatabase* web_database,
                   PersonalDataManager* personal_data,
                   browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD4(CreateAutofillProfileSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   WebDatabase* web_database,
                   PersonalDataManager* personal_data,
                   browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateBookmarkSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateExtensionSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD3(CreatePasswordSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   PasswordStore* password_store,
                   browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreatePreferenceSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateSessionSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateThemeSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD3(CreateTypedUrlSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   history::HistoryBackend* history_backend,
                   browser_sync::UnrecoverableErrorHandler* error_handler));

 private:
  SyncComponents MakeBookmarkSyncComponents();

  scoped_ptr<browser_sync::AssociatorInterface> bookmark_model_associator_;
  scoped_ptr<browser_sync::ChangeProcessor> bookmark_change_processor_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_MOCK_H__
