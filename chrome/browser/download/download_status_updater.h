// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_

#include <set>

#include "base/basictypes.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

// Keeps track of download progress for the entire browser.
class DownloadStatusUpdater
    : public content::DownloadManager::Observer,
      public content::DownloadItem::Observer {
 public:
  DownloadStatusUpdater();
  virtual ~DownloadStatusUpdater();

  // Fills in |*download_count| with the number of currently active downloads.
  // If we know the final size of all downloads, this routine returns true
  // with |*progress| set to the percentage complete of all in-progress
  // downloads.  Otherwise, it returns false.
  bool GetProgress(float* progress, int* download_count) const;

  // Add the specified DownloadManager to the list of managers for which
  // this object reports status.
  // The manager must not have previously been added to this updater.
  // The updater will automatically disassociate itself from the
  // manager when the manager is shutdown.
  void AddManager(content::DownloadManager* manager);

  // Methods inherited from content::DownloadManager::Observer.
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;
  virtual void SelectFileDialogDisplayed(
      content::DownloadManager* manager, int32 id) OVERRIDE;

  // Methods inherited from content::DownloadItem::Observer.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

 protected:
  // Update the app icon. Virtual to be overridable for testing.
  virtual void UpdateAppIconDownloadProgress();

 private:
  // Update the internal state tracking an item.
  void UpdateItem(content::DownloadItem* download);

  std::set<content::DownloadManager*> managers_;
  std::set<content::DownloadItem*> items_;

  DISALLOW_COPY_AND_ASSIGN(DownloadStatusUpdater);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_
