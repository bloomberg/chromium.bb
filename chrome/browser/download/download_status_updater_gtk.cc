// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "chrome/browser/ui/gtk/unity_service.h"

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    content::DownloadItem* download) {
  float progress = 0;
  int download_count = 0;
  GetProgress(&progress, &download_count);
  unity::SetDownloadCount(download_count);
  unity::SetProgressFraction(progress);
}
