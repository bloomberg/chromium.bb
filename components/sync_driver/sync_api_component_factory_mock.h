// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__

#include "base/memory/scoped_ptr.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_driver {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class DataTypeStatusTable;
class SyncClient;
}

class SyncApiComponentFactoryMock
    : public sync_driver::SyncApiComponentFactory {
 public:
  SyncApiComponentFactoryMock();
  SyncApiComponentFactoryMock(
      sync_driver::AssociatorInterface* model_associator,
      sync_driver::ChangeProcessor* change_processor);
  ~SyncApiComponentFactoryMock() override;

  MOCK_METHOD1(RegisterDataTypes, void(sync_driver::SyncClient*));
  MOCK_METHOD5(CreateDataTypeManager,
               sync_driver::DataTypeManager*(
                   const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
                   const sync_driver::DataTypeController::TypeMap*,
                   const sync_driver::DataTypeEncryptionHandler*,
                   browser_sync::SyncBackendHost*,
                   sync_driver::DataTypeManagerObserver* observer));
  MOCK_METHOD5(CreateSyncBackendHost,
               browser_sync::SyncBackendHost*(
                   const std::string& name,
                   sync_driver::SyncClient* sync_client,
                   invalidation::InvalidationService* invalidator,
                   const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
                   const base::FilePath& sync_folder));

  scoped_ptr<sync_driver::LocalDeviceInfoProvider>
      CreateLocalDeviceInfoProvider() override;
  void SetLocalDeviceInfoProvider(
      scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device);

  scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      scoped_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override;
  MOCK_METHOD2(CreateBookmarkSyncComponents,
      SyncComponents(sync_driver::SyncService* sync_service,
                     sync_driver::DataTypeErrorHandler* error_handler));
  MOCK_METHOD3(CreateTypedUrlSyncComponents,
               SyncComponents(
                   sync_driver::SyncService* sync_service,
                   history::HistoryBackend* history_backend,
                   sync_driver::DataTypeErrorHandler* error_handler));

 private:
  sync_driver::SyncApiComponentFactory::SyncComponents MakeSyncComponents();

  scoped_ptr<sync_driver::AssociatorInterface> model_associator_;
  scoped_ptr<sync_driver::ChangeProcessor> change_processor_;
  // LocalDeviceInfoProvider is initially owned by this class,
  // transferred to caller when CreateLocalDeviceInfoProvider is called.
  scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device_;
};

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__
