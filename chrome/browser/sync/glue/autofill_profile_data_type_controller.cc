// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service) : AutofillDataTypeController(
        profile_sync_factory,
        profile,
        sync_service) {}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

syncable::ModelType AutofillProfileDataTypeController::type() {
  return syncable::AUTOFILL_PROFILE;
}

const char* AutofillProfileDataTypeController::name() const {
  // For logging only.
  return "autofill_profile";
}

ProfileSyncFactory::SyncComponents
    AutofillProfileDataTypeController::CreateSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler) {
  return profile_sync_factory_->CreateAutofillProfileSyncComponents(
      profile_sync_service,
      web_database,
      personal_data,
      this);
}
}  // namepsace browser_sync

