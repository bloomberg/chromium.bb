// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include "components/sync_driver/sync_client.h"

class ProfileSyncComponentsFactoryImpl;

namespace browser_sync {

class ChromeSyncClient : public sync_driver::SyncClient {
 public:
  ChromeSyncClient(Profile* profile,
                   ProfileSyncComponentsFactoryImpl* component_factory);
  ~ChromeSyncClient() override;

  // SyncClient implementation.
  PrefService* GetPrefService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  history::HistoryService* GetHistoryService() override;
  scoped_refptr<password_manager::PasswordStore> GetPasswordStore() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<autofill::AutofillWebDataService> GetWebDataService() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  // TODO(zea): crbug.com/512768 Move the rest of these methods into
  // SyncApiComponentFactoryImpl (all of these methods currently delegate to
  // |component_factory_|), which will replace ProfileSyncComponentsFactoryImpl.
  scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      scoped_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override;
  void RegisterDataTypes(ProfileSyncService* pss) override;
  sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) override;
  browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      Profile* profile,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) override;
  scoped_ptr<sync_driver::LocalDeviceInfoProvider>
      CreateLocalDeviceInfoProvider() override;
  ProfileSyncComponentsFactory::SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) override;
  ProfileSyncComponentsFactory::SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      sync_driver::DataTypeErrorHandler* error_handler) override;

  // TODO(zea): crbug.com/512768 Convert this to SyncApiComponentFactoryImpl
  // and make it part of the SyncClient interface.
  ProfileSyncComponentsFactoryImpl* GetProfileSyncComponentsFactoryImpl();


 private:
  Profile* const profile_;
  ProfileSyncComponentsFactoryImpl* const component_factory_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
