// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
#pragma once

#include <string>
#include <utility>

#include "base/task.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"

class PersonalDataManager;
class PasswordStore;
class ProfileSyncService;
class WebDatabase;

namespace browser_sync {
class DataTypeManager;
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
  struct SyncComponents {
    browser_sync::AssociatorInterface* model_associator;
    browser_sync::ChangeProcessor* change_processor;
    SyncComponents(browser_sync::AssociatorInterface* ma,
                   browser_sync::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  virtual ~ProfileSyncFactory() {}

  // Instantiates and initializes a new ProfileSyncService.  Enabled
  // data types are registered with the service.  The return pointer
  // is owned by the caller.
  virtual ProfileSyncService* CreateProfileSyncService(
      const std::string& cros_user) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost and a
  // list of data type controllers.  The return pointer is owned by
  // the caller.
  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      browser_sync::SyncBackendHost* backend,
      const browser_sync::DataTypeController::TypeMap& controllers) = 0;

  // Instantiates both a model associator and change processor for the
  // app data type.  The pointers in the return struct are
  // owned by the caller.
  virtual SyncComponents CreateAppSyncComponents(
      ProfileSyncService* profile_sync_service,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // autofill data type.  The pointers in the return struct are owned
  // by the caller.
  virtual SyncComponents CreateAutofillSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // autofill data type.  The pointers in the return struct are owned
  // by the caller.
  virtual SyncComponents CreateAutofillProfileSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler) = 0;

  // Instantiates both a model associator and change processor for the
  // bookmark data type.  The pointers in the return struct are owned
  // by the caller.
  virtual SyncComponents CreateBookmarkSyncComponents(
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
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_FACTORY_H__
