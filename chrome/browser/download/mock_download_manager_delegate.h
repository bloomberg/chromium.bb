// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/download/download_manager_delegate.h"

class MockDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  virtual ~MockDownloadManagerDelegate();
  virtual void GetSaveDir(TabContents* tab_contents,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir) OVERRIDE;
  virtual void ChooseSavePath(const base::WeakPtr<SavePackage>& save_package,
                              const FilePath& suggested_path,
                              bool can_save_as_complete) OVERRIDE;
  virtual void ChooseDownloadPath(DownloadManager* download_manager,
                                  TabContents* tab_contents,
                                  const FilePath& suggested_path,
                                  void* data) OVERRIDE;
  virtual TabContents* GetAlternativeTabContentsToNotifyForDownload(
      DownloadManager* download_manager) OVERRIDE;
};

#endif  // CHROME_BROWSER_DOWNLOAD_MOCK_DOWNLOAD_MANAGER_DELEGATE_H_
