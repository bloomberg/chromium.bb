// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

SearchEngineDataTypeController::SearchEngineDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(syncer::SEARCH_ENGINES,
                           profile_sync_factory,
                           profile,
                           sync_service) {
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

  // Register a callback and continue when the TemplateURLService is loaded.
  template_url_subscription_ = turl_service->RegisterOnLoadedCallback(
      base::Bind(&SearchEngineDataTypeController::OnTemplateURLServiceLoaded,
                 this));

  return false;  // Don't continue Start.
}

void SearchEngineDataTypeController::OnTemplateURLServiceLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  template_url_subscription_.reset();
  OnModelLoaded();
}

}  // namespace browser_sync
