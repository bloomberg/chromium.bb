// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class CrxInstaller;
class DownloadHistory;
class DownloadPrefs;
class Profile;

namespace content {
class DownloadManager;
}

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<CrxInstaller*> {
  std::size_t operator()(CrxInstaller* const& p) const {
    return reinterpret_cast<std::size_t>(p);
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif

// This is the Chrome side helper for the download system.
class ChromeDownloadManagerDelegate
    : public base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>,
      public content::DownloadManagerDelegate,
      public content::NotificationObserver {
 public:
  explicit ChromeDownloadManagerDelegate(Profile* profile);

  void SetDownloadManager(content::DownloadManager* dm);

  // Should be called before the first call to ShouldCompleteDownload() to
  // disable SafeBrowsing checks for |item|.
  static void DisableSafeBrowsing(content::DownloadItem* item);

  virtual void Shutdown() OVERRIDE;
  virtual content::DownloadId GetNextId() OVERRIDE;
  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE;
  virtual void ChooseDownloadPath(content::DownloadItem* item) OVERRIDE;
  virtual FilePath GetIntermediatePath(const content::DownloadItem& item,
                                       bool* ok_to_overwrite) OVERRIDE;
  virtual content::WebContents*
      GetAlternativeWebContentsToNotifyForDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
  virtual bool ShouldCompleteDownload(
      content::DownloadItem* item,
      const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenDownload(content::DownloadItem* item) OVERRIDE;
  virtual bool GenerateFileHash() OVERRIDE;
  virtual void AddItemToPersistentStore(content::DownloadItem* item) OVERRIDE;
  virtual void UpdateItemInPersistentStore(
      content::DownloadItem* item) OVERRIDE;
  virtual void UpdatePathForItemInPersistentStore(
      content::DownloadItem* item,
      const FilePath& new_path) OVERRIDE;
  virtual void RemoveItemFromPersistentStore(
      content::DownloadItem* item) OVERRIDE;
  virtual void RemoveItemsFromPersistentStoreBetween(
      base::Time remove_begin,
      base::Time remove_end) OVERRIDE;
  virtual void GetSaveDir(content::WebContents* web_contents,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir,
                          bool* skip_dir_check) OVERRIDE;
  virtual void ChooseSavePath(
      content::WebContents* web_contents,
      const FilePath& suggested_path,
      const FilePath::StringType& default_extension,
      bool can_save_as_complete,
      const content::SavePackagePathPickedCallback& callback) OVERRIDE;

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }
  DownloadHistory* download_history() { return download_history_.get(); }

 protected:
  // So that test classes can inherit from this for override purposes.
  virtual ~ChromeDownloadManagerDelegate();

  // Returns the SafeBrowsing download protection service if it's
  // enabled. Returns NULL otherwise. Protected virtual for testing.
  virtual safe_browsing::DownloadProtectionService*
     GetDownloadProtectionService();

  // Returns true if this download should show the "dangerous file" warning.
  // Various factors are considered, such as the type of the file, whether a
  // user action initiated the download, and whether the user has explicitly
  // marked the file type as "auto open". Protected virtual for testing.
  virtual bool IsDangerousFile(const content::DownloadItem& download,
                               const FilePath& suggested_path,
                               bool visited_referrer_before);

  // So that test classes that inherit from this for override purposes
  // can call back into the DownloadManager.
  scoped_refptr<content::DownloadManager> download_manager_;

 private:
  friend class base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callback function after url is checked with safebrowsing service.
  void CheckDownloadUrlDone(
      int32 download_id,
      safe_browsing::DownloadProtectionService::DownloadCheckResult result);

  // Callback function after the DownloadProtectionService completes.
  void CheckClientDownloadDone(
      int32 download_id,
      safe_browsing::DownloadProtectionService::DownloadCheckResult result);

  // Callback function after we check whether the referrer URL has been visited
  // before today. Determines the danger state of the download based on the file
  // type and |visited_referrer_before|. Generates a target path for the
  // download. Invokes |CheckIfSuggestedPathExists| on the FILE thread to check
  // the target path.
  void CheckVisitedReferrerBeforeDone(int32 download_id,
                                      content::DownloadDangerType danger_type,
                                      bool visited_referrer_before);

#if defined (OS_CHROMEOS)
  // GDataDownloadObserver::SubstituteGDataDownloadPath callback.
  // Posts CheckIfSuggestedPathExists on the FILE thread.
  void SubstituteGDataDownloadPathCallback(
      int32 download_id,
      bool should_prompt,
      bool is_forced_path,
      content::DownloadDangerType danger_type,
      const FilePath& unverified_path);
#endif

  // Called on the FILE thread to check whether the suggested file path exists.
  // We don't check if the file exists on the UI thread to avoid UI stalls from
  // interacting with the file system. Creates the default download directory
  // specified in |default_path| if it doesn't exist. Uniquifies |unverified
  // path| if necessary. The verified path is then passed down to
  // |OnPathExistenceAvailable|.
  void CheckIfSuggestedPathExists(int32 download_id,
                                  const FilePath& unverified_path,
                                  bool should_prompt,
                                  bool is_forced_path,
                                  content::DownloadDangerType danger_type,
                                  const FilePath& default_path);

  // Called on the UI thread once it's determined whether the suggested file
  // path exists. Updates the download identified by |download_id| with the
  // |target_path|, |target_disposition| and |danger_type|.
  void OnPathExistenceAvailable(
      int32 download_id,
      const FilePath& target_path,
      content::DownloadItem::TargetDisposition disposition,
      content::DownloadDangerType danger_type);

  // Callback from history system.
  void OnItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  // Check policy of whether we should open this download with a web intents
  // dispatch.
  bool ShouldOpenWithWebIntents(const content::DownloadItem* item);

  // Open the given item with a web intent dispatch.
  void OpenWithWebIntent(const content::DownloadItem* item);

  // Internal gateways for ShouldCompleteDownload().
  bool IsDownloadReadyForCompletion(
      content::DownloadItem* item,
      const base::Closure& internal_complete_callback);
  void ShouldCompleteDownloadInternal(
    int download_id,
    const base::Closure& user_complete_callback);

  Profile* profile_;
  int next_download_id_;
  scoped_ptr<DownloadPrefs> download_prefs_;
  scoped_ptr<DownloadHistory> download_history_;

  // Maps from pending extension installations to DownloadItem IDs.
  typedef base::hash_map<CrxInstaller*, int> CrxInstallerMap;
  CrxInstallerMap crx_installers_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
