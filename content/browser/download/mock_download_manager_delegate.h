// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/download/download_manager_delegate.h"

class MockDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  virtual ~MockDownloadManagerDelegate();
  virtual void Shutdown() OVERRIDE;
  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE;
  virtual void ChooseDownloadPath(TabContents* tab_contents,
                                  const FilePath& suggested_path,
                                  void* data) OVERRIDE;
  virtual bool OverrideIntermediatePath(DownloadItem* item,
                                        FilePath* intermediate_path) OVERRIDE;
  virtual TabContents* GetAlternativeTabContentsToNotifyForDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
  virtual bool ShouldOpenDownload(DownloadItem* item) OVERRIDE;
  virtual bool ShouldCompleteDownload(DownloadItem* item) OVERRIDE;
  virtual bool GenerateFileHash() OVERRIDE;
  virtual void AddItemToPersistentStore(DownloadItem* item) OVERRIDE;
  virtual void UpdateItemInPersistentStore(DownloadItem* item) OVERRIDE;
  virtual void UpdatePathForItemInPersistentStore(
      DownloadItem* item,
      const FilePath& new_path) OVERRIDE;
  virtual void RemoveItemFromPersistentStore(DownloadItem* item) OVERRIDE;
  virtual void RemoveItemsFromPersistentStoreBetween(
      const base::Time remove_begin,
      const base::Time remove_end) OVERRIDE;
  virtual void GetSaveDir(TabContents* tab_contents,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir) OVERRIDE;
  virtual void ChooseSavePath(const base::WeakPtr<SavePackage>& save_package,
                              const FilePath& suggested_path,
                              bool can_save_as_complete) OVERRIDE;
  virtual void DownloadProgressUpdated() OVERRIDE;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
