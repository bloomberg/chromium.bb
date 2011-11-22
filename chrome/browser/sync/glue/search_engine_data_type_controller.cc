// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

SearchEngineDataTypeController::SearchEngineDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

SearchEngineDataTypeController::~SearchEngineDataTypeController() {
}

syncable::ModelType SearchEngineDataTypeController::type() const {
  return syncable::SEARCH_ENGINES;
}

void SearchEngineDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED, type);
  registrar_.RemoveAll();
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = ASSOCIATING;
  Associate();
}

// We want to start the TemplateURLService before we begin associating.
bool SearchEngineDataTypeController::StartModels() {
  // If the TemplateURLService is loaded, continue with association.
  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (turl_service && turl_service->loaded()) {
    return true;  // Continue to Associate().
  }

  // Add an observer and continue when the TemplateURLService is loaded.
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                 content::Source<TemplateURLService>(turl_service));
  return false;  // Don't continue Start.
}

// Cleanup for our extra registrar usage.
void SearchEngineDataTypeController::CleanUpState() {
  registrar_.RemoveAll();
}

void SearchEngineDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateSearchEngineSyncComponents(sync_service_,
                                                              this);
  set_model_associator(sync_components.model_associator);
  generic_change_processor_.reset(static_cast<GenericChangeProcessor*>(
      sync_components.change_processor));
}

GenericChangeProcessor* SearchEngineDataTypeController::change_processor()
    const {
  return generic_change_processor_.get();
}

void SearchEngineDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.SearchEngineRunFailures", 1);
}

void SearchEngineDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.SearchEngineAssociationTime", time);
}

void SearchEngineDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.SearchEngineStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
