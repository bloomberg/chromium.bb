// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
}

class ProfileSyncComponentsFactoryMock : public ProfileSyncComponentsFactory {
 public:
  ProfileSyncComponentsFactoryMock();
  ProfileSyncComponentsFactoryMock(
      browser_sync::AssociatorInterface* model_associator,
      browser_sync::ChangeProcessor* change_processor);
  virtual ~ProfileSyncComponentsFactoryMock();

  MOCK_METHOD1(CreateProfileSyncService,
               ProfileSyncService*(const std::string&));
  MOCK_METHOD1(RegisterDataTypes, void(ProfileSyncService*));
  MOCK_METHOD2(CreateDataTypeManager,
               browser_sync::DataTypeManager*(
                   browser_sync::SyncBackendHost*,
                   const browser_sync::DataTypeController::TypeMap*));
  MOCK_METHOD3(CreateGenericChangeProcessor,
      browser_sync::GenericChangeProcessor*(
          ProfileSyncService* profile_sync_service,
          browser_sync::UnrecoverableErrorHandler* error_handler,
          const base::WeakPtr<SyncableService>& local_service));
  MOCK_METHOD0(CreateSharedChangeProcessor,
      browser_sync::SharedChangeProcessor*());
  MOCK_METHOD2(CreateAppSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_CONST_METHOD1(GetAutofillProfileSyncableService,
                     base::WeakPtr<SyncableService>(
                         WebDataService* web_data_service));
  MOCK_CONST_METHOD1(GetAutocompleteSyncableService,
                     base::WeakPtr<SyncableService>(
                         WebDataService* web_data_service));
  MOCK_METHOD2(CreateBookmarkSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateExtensionSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD4(CreateExtensionOrAppSettingSyncComponents,
      SyncComponents(syncable::ModelType model_type,
                     SyncableService* settings_service,
                     ProfileSyncService* profile_sync_service,
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
  MOCK_METHOD2(CreateSearchEngineSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   browser_sync::UnrecoverableErrorHandler* error_handler));
  MOCK_METHOD2(CreateAppNotificationSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   browser_sync::UnrecoverableErrorHandler* error_handler));

 private:
  SyncComponents MakeSyncComponents();

  scoped_ptr<browser_sync::AssociatorInterface> model_associator_;
  scoped_ptr<browser_sync::ChangeProcessor> change_processor_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
