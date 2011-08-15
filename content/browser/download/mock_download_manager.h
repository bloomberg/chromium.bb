// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
#pragma once

#include "content/browser/download/download_manager.h"

class DownloadStatusUpdater;
class DownloadItem;

class MockDownloadManager : public DownloadManager {
 public:
  explicit MockDownloadManager(DownloadManagerDelegate* delegate,
                               DownloadStatusUpdater* updater)
      : DownloadManager(delegate, updater) {
  }

  // Override some functions.
  virtual void UpdateHistoryForDownload(DownloadItem*) { }
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_H_
