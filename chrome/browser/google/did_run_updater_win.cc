// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/did_run_updater_win.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

DidRunUpdater::DidRunUpdater() : system_level_(false) {
  base::FilePath exe_path;
  if (PathService::Get(base::FILE_EXE, &exe_path))
    system_level_ = !InstallUtil::IsPerUserInstall(exe_path.value().c_str());

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

DidRunUpdater::~DidRunUpdater() {
}

void DidRunUpdater::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  GoogleUpdateSettings::UpdateDidRunState(true, system_level_);
}
