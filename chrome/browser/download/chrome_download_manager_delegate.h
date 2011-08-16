// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "content/browser/download/download_manager_delegate.h"

class DownloadItem;
class DownloadManager;
struct DownloadStateInfo;

// Browser's download manager: manages all downloads and destination view.
class ChromeDownloadManagerDelegate
    : public base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>,
      public DownloadManagerDelegate {
 public:
  ChromeDownloadManagerDelegate();

  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE;
  virtual void ChooseDownloadPath(TabContents* tab_contents,
                                  const FilePath& suggested_path,
                                  void* data) OVERRIDE;
  virtual TabContents* GetAlternativeTabContentsToNotifyForDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
  virtual void GetSaveDir(TabContents* tab_contents,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir) OVERRIDE;
  virtual void ChooseSavePath(const base::WeakPtr<SavePackage>& save_package,
                              const FilePath& suggested_path,
                              bool can_save_as_complete) OVERRIDE;

  void set_download_manager(DownloadManager* dm) { download_manager_ = dm; }

 private:
  friend class base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>;
  virtual ~ChromeDownloadManagerDelegate();

  // Callback function after url is checked with safebrowsing service.
  void CheckDownloadUrlDone(int32 download_id, bool is_dangerous_url);

  // Callback function after we check whether the referrer URL has been visited
  // before today.
  void CheckVisitedReferrerBeforeDone(int32 download_id,
                                      bool visited_referrer_before);

  // Called on the download thread to check whether the suggested file path
  // exists.  We don't check if the file exists on the UI thread to avoid UI
  // stalls from interacting with the file system.
  void CheckIfSuggestedPathExists(int32 download_id,
                                  DownloadStateInfo state,
                                  const FilePath& default_path);

  // Called on the UI thread once it's determined whether the suggested file
  // path exists.
  void OnPathExistenceAvailable(int32 download_id,
                                const DownloadStateInfo& new_state);

  // Returns true if this download should show the "dangerous file" warning.
  // Various factors are considered, such as the type of the file, whether a
  // user action initiated the download, and whether the user has explicitly
  // marked the file type as "auto open".
  bool IsDangerousFile(const DownloadItem& download,
                       const DownloadStateInfo& state,
                       bool visited_referrer_before);

  scoped_refptr<DownloadManager> download_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
