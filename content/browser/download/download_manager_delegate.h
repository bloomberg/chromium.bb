// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"

class FilePath;
class TabContents;
class SavePackage;

// Browser's download manager: manages all downloads and destination view.
class DownloadManagerDelegate {
 public:
  // Notifies the delegate that a download is starting. The delegate can return
  // false to delay the start of the download, in which case it should call
  // DownloadManager::RestartDownload when it's ready.
  virtual bool ShouldStartDownload(int32 download_id) = 0;

  // Asks the user for the path for a download. The delegate calls
  // DownloadManager::FileSelected or DownloadManager::FileSelectionCanceled to
  // give the answer.
  virtual void ChooseDownloadPath(TabContents* tab_contents,
                                  const FilePath& suggested_path,
                                  void* data) = 0;

  // Called when the download system wants to alert a TabContents that a
  // download has started, but the TabConetnts has gone away. This lets an
  // embedder return an alternative TabContents. The embedder can return NULL.
  virtual TabContents* GetAlternativeTabContentsToNotifyForDownload() = 0;

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) = 0;

  // Retrieve the directories to save html pages and downloads to.
  virtual void GetSaveDir(TabContents* tab_contents,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir) = 0;

  // Asks the user for the path to save a page. The delegate calls
  // SavePackage::OnPathPicked to give the answer.
  virtual void ChooseSavePath(const base::WeakPtr<SavePackage>& save_package,
                              const FilePath& suggested_path,
                              bool can_save_as_complete) = 0;

 protected:
  DownloadManagerDelegate() {}
  virtual ~DownloadManagerDelegate() {}

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegate);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_DELEGATE_H_
