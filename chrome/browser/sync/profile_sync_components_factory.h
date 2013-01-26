// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"

class FailedDatatypesHandler;
class PasswordStore;
class ProfileSyncService;
class WebDataService;

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeManager;
class DataTypeManagerObserver;
class GenericChangeProcessor;
class SharedChangeProcessor;
class SyncBackendHost;
class DataTypeErrorHandler;
}

namespace syncer {
class DataTypeDebugInfoListener;
class SyncableService;
}

namespace history {
class HistoryBackend;
}

// Factory class for all profile sync related classes.
class ProfileSyncComponentsFactory {
 public:
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
    browser_sync::AssociatorInterface* model_associator;
    browser_sync::ChangeProcessor* change_processor;
    SyncComponents(browser_sync::AssociatorInterface* ma,
                   browser_sync::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  virtual ~ProfileSyncComponentsFactory() {}

  // Creates and registers enabled datatypes with the provided
  // ProfileSyncService.
  virtual void RegisterDataTypes(ProfileSyncService* pss) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost, a list of data
  // type controllers and a DataTypeManagerObserver.  The return pointer is
  // owned by the caller.
  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap* controllers,
      browser_sync::DataTypeManagerObserver* observer,
      const FailedDatatypesHandler* failed_datatypes_handler) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual browser_sync::GenericChangeProcessor* CreateGenericChangeProcessor(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result) = 0;

  virtual browser_sync::SharedChangeProcessor*
      CreateSharedChangeProcessor() = 0;

  // Returns a weak pointer to the syncable service specified by |type|.
  // Weak pointer may be unset if service is already destroyed.
  // Note: Should only be called on the same thread on which a datatype resides.
  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) = 0;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
  virtual SyncComponents CreatePasswordSyncComponents(
      ProfileSyncService* profile_sync_service,
      PasswordStore* password_store,
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
  virtual SyncComponents CreateSessionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__
