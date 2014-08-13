// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    syncer::ModelType type,
    sync_driver::SyncApiComponentFactory* sync_factory,
    Profile* profile)
    : UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          type,
          sync_factory),
      profile_(profile) {
  DCHECK(type == syncer::EXTENSIONS || type == syncer::APPS);
}

ExtensionDataTypeController::~ExtensionDataTypeController() {}

bool ExtensionDataTypeController::StartModels() {
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(true);
  return true;
}

}  // namespace browser_sync
