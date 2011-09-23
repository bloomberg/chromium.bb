// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/webdata/web_data_service.h"

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile)
    : AutofillDataTypeController(profile_sync_factory, profile) {}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

syncable::ModelType AutofillProfileDataTypeController::type() const {
  return syncable::AUTOFILL_PROFILE;
}

void AutofillProfileDataTypeController::CreateSyncComponents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory()->
          CreateAutofillProfileSyncComponents(
          profile_sync_service(),
          web_data_service()->GetDatabase(),
          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void AutofillProfileDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_COUNTS("Sync.AutofillProfileRunFailures", 1);
}

void AutofillProfileDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_TIMES("Sync.AutofillProfileAssociationTime", time);
}

void AutofillProfileDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.AutofillProfileStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namepsace browser_sync
