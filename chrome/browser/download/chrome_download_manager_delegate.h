// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "content/browser/download/download_manager_delegate.h"

class DownloadHistory;
class DownloadItem;
class DownloadManager;
class DownloadPrefs;
class Profile;
struct DownloadHistoryInfo;
struct DownloadStateInfo;

// This is the Chrome side helper for the download system.
class ChromeDownloadManagerDelegate
    : public base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>,
      public DownloadManagerDelegate {
 public:
  explicit ChromeDownloadManagerDelegate(Profile* profile);

  void SetDownloadManager(DownloadManager* dm);

  virtual void Shutdown() OVERRIDE;
  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE;
  virtual void ChooseDownloadPath(TabContents* tab_contents,
                                  const FilePath& suggested_path,
                                  void* data) OVERRIDE;
  virtual TabContents* GetAlternativeTabContentsToNotifyForDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
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

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }
  DownloadHistory* download_history() { return download_history_.get(); }

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

  // Callback from history system.
  void OnItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  Profile* profile_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_ptr<DownloadPrefs> download_prefs_;
  scoped_ptr<DownloadHistory> download_history_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
