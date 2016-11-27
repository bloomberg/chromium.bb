// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/features/features.h"

class DownloadPrefs;
class Profile;

namespace content {
class DownloadManager;
}

namespace extensions {
class CrxInstaller;
}

// This is the Chrome side helper for the download system.
class ChromeDownloadManagerDelegate
    : public content::DownloadManagerDelegate,
      public content::NotificationObserver,
      public DownloadTargetDeterminerDelegate {
 public:
  explicit ChromeDownloadManagerDelegate(Profile* profile);
  ~ChromeDownloadManagerDelegate() override;

  // Should be called before the first call to ShouldCompleteDownload() to
  // disable SafeBrowsing checks for |item|.
  static void DisableSafeBrowsing(content::DownloadItem* item);

  void SetDownloadManager(content::DownloadManager* dm);

  // Callbacks passed to GetNextId() will not be called until the returned
  // callback is called.
  content::DownloadIdCallback GetDownloadIdReceiverCallback();

  // content::DownloadManagerDelegate
  void Shutdown() override;
  void GetNextId(const content::DownloadIdCallback& callback) override;
  bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) override;
  bool ShouldOpenFileBasedOnExtension(const base::FilePath& path) override;
  bool ShouldCompleteDownload(content::DownloadItem* item,
                              const base::Closure& complete_callback) override;
  bool ShouldOpenDownload(
      content::DownloadItem* item,
      const content::DownloadOpenDelayedCallback& callback) override;
  bool GenerateFileHash() override;
  void GetSaveDir(content::BrowserContext* browser_context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir,
                  bool* skip_dir_check) override;
  void ChooseSavePath(
      content::WebContents* web_contents,
      const base::FilePath& suggested_path,
      const base::FilePath::StringType& default_extension,
      bool can_save_as_complete,
      const content::SavePackagePathPickedCallback& callback) override;
  void SanitizeSavePackageResourceName(base::FilePath* filename) override;
  void OpenDownload(content::DownloadItem* download) override;
  void ShowDownloadInShell(content::DownloadItem* download) override;
  void CheckForFileExistence(
      content::DownloadItem* download,
      const content::CheckForFileExistenceCallback& callback) override;
  std::string ApplicationClientIdForFileScanning() const override;

  // Opens a download using the platform handler. DownloadItem::OpenDownload,
  // which ends up being handled by OpenDownload(), will open a download in the
  // browser if doing so is preferred.
  void OpenDownloadUsingPlatformHandler(content::DownloadItem* download);

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }

 protected:
  // So that test classes that inherit from this for override purposes
  // can call back into the DownloadManager.
  content::DownloadManager* download_manager_;

  virtual safe_browsing::DownloadProtectionService*
      GetDownloadProtectionService();

  // DownloadTargetDeterminerDelegate. Protected for testing.
  void NotifyExtensions(content::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        const NotifyExtensionsCallback& callback) override;
  void ReserveVirtualPath(
      content::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      const ReservedPathCallback& callback) override;
  void PromptUserForDownloadPath(content::DownloadItem* download,
                                 const base::FilePath& suggested_virtual_path,
                                 const FileSelectedCallback& callback) override;
  void DetermineLocalPath(content::DownloadItem* download,
                          const base::FilePath& virtual_path,
                          const LocalPathCallback& callback) override;
  void CheckDownloadUrl(content::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        const CheckDownloadUrlCallback& callback) override;
  void GetFileMimeType(const base::FilePath& path,
                       const GetFileMimeTypeCallback& callback) override;

 private:
  friend class base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>;

  typedef std::vector<content::DownloadIdCallback> IdCallbackVector;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Callback function after the DownloadProtectionService completes.
  void CheckClientDownloadDone(
      uint32_t download_id,
      safe_browsing::DownloadProtectionService::DownloadCheckResult result);

  // Internal gateways for ShouldCompleteDownload().
  bool IsDownloadReadyForCompletion(
      content::DownloadItem* item,
      const base::Closure& internal_complete_callback);
  void ShouldCompleteDownloadInternal(
      uint32_t download_id,
      const base::Closure& user_complete_callback);

  void SetNextId(uint32_t id);

  void ReturnNextId(const content::DownloadIdCallback& callback);

  void OnDownloadTargetDetermined(
      int32_t download_id,
      const content::DownloadTargetCallback& callback,
      std::unique_ptr<DownloadTargetInfo> target_info);

  // Returns true if |path| should open in the browser.
  bool IsOpenInBrowserPreferreredForFile(const base::FilePath& path);

  Profile* profile_;
  uint32_t next_download_id_;
  IdCallbackVector id_callbacks_;
  std::unique_ptr<DownloadPrefs> download_prefs_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Maps from pending extension installations to DownloadItem IDs.
  typedef base::hash_map<extensions::CrxInstaller*,
      content::DownloadOpenDelayedCallback> CrxInstallerMap;
  CrxInstallerMap crx_installers_;
#endif

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ChromeDownloadManagerDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
