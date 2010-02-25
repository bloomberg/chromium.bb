// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/preference_change_processor.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/common/chrome_switches.h"

using browser_sync::BookmarkChangeProcessor;
using browser_sync::BookmarkDataTypeController;
using browser_sync::BookmarkModelAssociator;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::PreferenceChangeProcessor;
using browser_sync::PreferenceDataTypeController;
using browser_sync::PreferenceModelAssociator;

ProfileSyncFactoryImpl::ProfileSyncFactoryImpl(
    Profile* profile,
    CommandLine* command_line)
    : profile_(profile), command_line_(command_line) {
}

ProfileSyncService* ProfileSyncFactoryImpl::CreateProfileSyncService() {
  ProfileSyncService* pss =
      new ProfileSyncService(this,
                             profile_,
                             browser_defaults::kBootstrapSyncAuthentication);

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncBookmarks)) {
    pss->RegisterDataTypeController(
        new BookmarkDataTypeController(this, profile_, pss));
  }

  // Preference sync is disabled by default.  Register only if
  // explicitly enabled.
  if (command_line_->HasSwitch(switches::kEnableSyncPreferences)) {
    pss->RegisterDataTypeController(
        new PreferenceDataTypeController(this, pss));
  }

  return pss;
}

DataTypeManager* ProfileSyncFactoryImpl::CreateDataTypeManager(
    const DataTypeController::TypeMap& controllers) {
  return new DataTypeManagerImpl(controllers);
}

ProfileSyncFactory::SyncComponents
ProfileSyncFactoryImpl::CreateBookmarkSyncComponents(
    ProfileSyncService* profile_sync_service) {
  BookmarkModelAssociator* model_associator =
      new BookmarkModelAssociator(profile_sync_service);
  BookmarkChangeProcessor* change_processor =
      new BookmarkChangeProcessor(model_associator,
                                  profile_sync_service);
  return SyncComponents(model_associator, change_processor);
}

ProfileSyncFactory::SyncComponents
ProfileSyncFactoryImpl::CreatePreferenceSyncComponents(
    ProfileSyncService* profile_sync_service) {
  PreferenceModelAssociator* model_associator =
      new PreferenceModelAssociator(profile_sync_service);
  PreferenceChangeProcessor* change_processor =
      new PreferenceChangeProcessor(model_associator,
                                    profile_sync_service);
  return SyncComponents(model_associator, change_processor);
}
