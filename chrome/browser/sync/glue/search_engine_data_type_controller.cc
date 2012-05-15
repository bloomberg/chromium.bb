// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

SearchEngineDataTypeController::SearchEngineDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(syncable::SEARCH_ENGINES,
                           profile_sync_factory,
                           profile,
                           sync_service) {
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

SearchEngineDataTypeController::~SearchEngineDataTypeController() {}

// We want to start the TemplateURLService before we begin associating.
bool SearchEngineDataTypeController::StartModels() {
  // If the TemplateURLService is loaded, continue with association. We force
  // a load here to prevent the rest of Sync from waiting on
  // TemplateURLService's lazy load.
  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(turl_service);
  turl_service->Load();
  if (turl_service->loaded()) {
    return true;  // Continue to Associate().
  }

  // Add an observer and continue when the TemplateURLService is loaded.
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                 content::Source<TemplateURLService>(turl_service));
  return false;  // Don't continue Start.
}

// Cleanup for our extra registrar usage.
void SearchEngineDataTypeController::StopModels() {
  registrar_.RemoveAll();
}

}  // namespace browser_sync
