// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync_driver/data_type_controller.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_service.h"
#include "sync/internal_api/public/base/model_type.h"

namespace base {
class FilePath;
}  // namespace base

namespace browser_sync {
class SyncBackendHost;
}  // namespace browser_sync

namespace history {
class HistoryBackend;
}

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {
class DataTypeDebugInfoListener;
class SyncableService;

struct UserShare;
}  // namespace syncer

namespace sync_driver {

class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class DataTypeErrorHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class DataTypeStatusTable;
class GenericChangeProcessor;
class LocalDeviceInfoProvider;
class SyncPrefs;
class SyncClient;
class SyncService;

// This factory provides sync driver code with the model type specific sync/api
// service (like SyncableService) implementations.
class SyncApiComponentFactory {
 public:
  virtual ~SyncApiComponentFactory() {}
  // Callback to allow platform-specific datatypes to register themselves as
  // data type controllers.
  // |disabled_types| and |enabled_types| control the disable/enable state of
  // types that are on or off by default (respectively).
  typedef base::Callback<void(sync_driver::SyncService* sync_service,
                              syncer::ModelTypeSet disabled_types,
                              syncer::ModelTypeSet enabled_types)>
      RegisterDataTypesMethod;

  // The various factory methods for the data type model associators
  // and change processors all return this struct.  This is needed
  // because the change processors typically require a type-specific
  // model associator at construction time.
  //
  // Note: This interface is deprecated in favor of the SyncableService API.
  // New datatypes that do not live on the UI thread should directly return a
  // weak pointer to a syncer::SyncableService. All others continue to return
  // SyncComponents. It is safe to assume that the factory methods below are
  // called on the same thread in which the datatype resides.
  //
  // TODO(zea): Have all datatypes using the new API switch to returning
  // SyncableService weak pointers instead of SyncComponents (crbug.com/100114).
  struct SyncComponents {
    sync_driver::AssociatorInterface* model_associator;
    sync_driver::ChangeProcessor* change_processor;
    SyncComponents(sync_driver::AssociatorInterface* ma,
                   sync_driver::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  // Creates and registers enabled datatypes with the provided SyncClient.
  virtual void RegisterDataTypes(
      sync_driver::SyncService* sync_service,
      const RegisterDataTypesMethod& register_platform_types_method) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost, a list of data
  // type controllers and a DataTypeManagerObserver.  The return pointer is
  // owned by the caller.
  virtual sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual std::unique_ptr<sync_driver::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() = 0;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      sync_driver::SyncService* sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) = 0;

  // Creates attachment service.
  // Note: Should only be called from the model type thread.
  //
  // |store_birthday| is the store birthday.  Must not be empty.
  //
  // |model_type| is the model type this AttachmentService will be used with.
  //
  // |delegate| is optional delegate for AttachmentService to notify about
  // asynchronous events (AttachmentUploaded). Pass NULL if delegate is not
  // provided. AttachmentService doesn't take ownership of delegate, the pointer
  // must be valid throughout AttachmentService lifetime.
  virtual std::unique_ptr<syncer::AttachmentService> CreateAttachmentService(
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) = 0;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
