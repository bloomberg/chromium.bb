// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_OBSERVER_WIN_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_OBSERVER_WIN_H_

#include <set>

#include "base/basictypes.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

// Tracks download item completions and notifies any interested parties
// (Windows 8 metro) about the same. On Windows 8 we display metro style
// notifications when a download completes.
class DownloadCompletionObserver
    : public content::DownloadManager::Observer,
      public content::DownloadItem::Observer {
 public:
  explicit DownloadCompletionObserver(content::DownloadManager* manager);
  ~DownloadCompletionObserver();

  // Methods inherited from content::DownloadManager::Observer.
  virtual void OnDownloadCreated(
      content::DownloadManager* manager, content::DownloadItem* download)
      OVERRIDE;
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;

  // Methods inherited from content::DownloadItem::Observer.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

 private:
  void ClearDownloadItems();

  std::set<content::DownloadItem*> download_items_;

  DISALLOW_COPY_AND_ASSIGN(DownloadCompletionObserver);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_OBSERVER_WIN_H_
