// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
#pragma once

#include <string>

#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"

class PasswordStore;
class ProfileSyncService;
class SyncableService;
class WebDataService;

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeManager;
class GenericChangeProcessor;
class SharedChangeProcessor;
class SyncBackendHost;
class UnrecoverableErrorHandler;
}

namespace history {
class HistoryBackend;
};

// Factory class for all profile sync related classes.
class ProfileSyncFactory {
 public:
  // The various factory methods for the data type model associators
  // and change processors all return this struct.  This is needed
  // because the change processors typically require a type-specific
  // model associator at construction time.
  //
  // Note: This interface is deprecated in favor of the SyncableService API.
  // New datatypes that do not live on the UI thread should directly return a
  // weak pointer to a SyncableService. All others continue to return
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

  virtual ~ProfileSyncFactory() {}

  // Instantiates a new ProfileSyncService. The return pointer is owned by the
  // caller.
  virtual ProfileSyncService* CreateProfileSyncService(
      const std::string& cros_user) = 0;

  // Creates and registers enabled datatypes with the provided
  // ProfileSyncService.
  virtual void RegisterDataTypes(ProfileSyncService* pss) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost and a
  // list of data type controllers.  The return pointer is owned by
  // the caller.
  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap* controllers) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual browser_sync::GenericChangeProcessor* CreateGenericChangeProcessor(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler,
      const base::WeakPtr<SyncableService>& local_service) = 0;

  virtual browser_sync::SharedChangeProcessor*
      CreateSharedChangeProcessor() = 0;

  // Instantiates both a model associator and change processor for the
  // app data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateAppSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Returns a weak pointer to the SyncableService associated with the datatype.
  // The SyncableService is not owned by Sync, but by the backend service
  // itself.
  virtual base::WeakPtr<SyncableService> GetAutofillProfileSyncableService(
      WebDataService* web_data_service) const = 0;

  // Returns a weak pointer to the SyncableService associated with the datatype.
  // The SyncableService is not owned by Sync, but by the backend service
  // itself.
  virtual base::WeakPtr<SyncableService> GetAutocompleteSyncableService(
      WebDataService* web_data_service) const = 0;

  // Instantiates both a model associator and change processor for the
  // bookmark data type.  The pointers in the return struct are owned
  // by the caller.
  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // extension or app setting data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateExtensionOrAppSettingSyncComponents(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncable::ModelType type,
      SyncableService* settings_service,
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // extension data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateExtensionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // password data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreatePasswordSyncComponents(
      ProfileSyncService* profile_sync_service,
      PasswordStore* password_store,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // preference data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreatePreferenceSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // theme data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateThemeSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // typed_url data type.  The pointers in the return struct are owned
  // by the caller.
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // session data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateSessionSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the search
  // engine data type.  The pointers in the return struct are owned by the
  // caller.
  virtual SyncComponents CreateSearchEngineSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the app
  // notification data type.  The pointers in the return struct are owned by the
  // caller.
  virtual SyncComponents CreateAppNotificationSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
