// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class DownloadPrefs;
class PrefRegistrySyncable;
class Profile;

namespace content {
class DownloadManager;
}

namespace extensions {
class CrxInstaller;
}

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<extensions::CrxInstaller*> {
  std::size_t operator()(extensions::CrxInstaller* const& p) const {
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
  // Callback type used with ChooseDownloadPath(). The callback should be
  // invoked with the user-selected path as the argument. If the file selection
  // was canceled, the argument should be the empty path.
  typedef base::Callback<void(const base::FilePath&)> FileSelectedCallback;

  explicit ChromeDownloadManagerDelegate(Profile* profile);

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Should be called before the first call to ShouldCompleteDownload() to
  // disable SafeBrowsing checks for |item|.
  static void DisableSafeBrowsing(content::DownloadItem* item);

  void SetDownloadManager(content::DownloadManager* dm);

  // content::DownloadManagerDelegate
  virtual void Shutdown() OVERRIDE;
  virtual content::DownloadId GetNextId() OVERRIDE;
  virtual bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) OVERRIDE;
  virtual content::WebContents*
      GetAlternativeWebContentsToNotifyForDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(
      const base::FilePath& path) OVERRIDE;
  virtual bool ShouldCompleteDownload(
      content::DownloadItem* item,
      const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenDownload(
      content::DownloadItem* item,
      const content::DownloadOpenDelayedCallback& callback) OVERRIDE;
  virtual bool GenerateFileHash() OVERRIDE;
  virtual void GetSaveDir(content::BrowserContext* browser_context,
                          base::FilePath* website_save_dir,
                          base::FilePath* download_save_dir,
                          bool* skip_dir_check) OVERRIDE;
  virtual void ChooseSavePath(
      content::WebContents* web_contents,
      const base::FilePath& suggested_path,
      const base::FilePath::StringType& default_extension,
      bool can_save_as_complete,
      const content::SavePackagePathPickedCallback& callback) OVERRIDE;
  virtual void OpenDownload(content::DownloadItem* download) OVERRIDE;
  virtual void ShowDownloadInShell(content::DownloadItem* download) OVERRIDE;
  virtual void CheckForFileExistence(
      content::DownloadItem* download,
      const content::CheckForFileExistenceCallback& callback) OVERRIDE;

  // Clears the last directory chosen by the user in response to a file chooser
  // prompt. Called when clearing recent history.
  void ClearLastDownloadPath();

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }

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
                               const base::FilePath& suggested_path,
                               bool visited_referrer_before);

  // Obtains a path reservation by calling
  // DownloadPathReservationTracker::GetReservedPath(). Protected virtual for
  // testing.
  virtual void GetReservedPath(
      content::DownloadItem& download,
      const base::FilePath& target_path,
      const base::FilePath& default_download_path,
      bool should_uniquify_path,
      const DownloadPathReservationTracker::ReservedPathCallback& callback);

  // Displays the file chooser dialog to prompt the user for the download
  // location for |item|. |suggested_path| will be used as the initial download
  // path. Once a location is available |callback| will be invoked with the
  // selected full path. If the user cancels the dialog, then an empty FilePath
  // will be passed into |callback|. Protected virtual for testing.
  virtual void ChooseDownloadPath(content::DownloadItem* item,
                                  const base::FilePath& suggested_path,
                                  const FileSelectedCallback& callback);

  // So that test classes that inherit from this for override purposes
  // can call back into the DownloadManager.
  scoped_refptr<content::DownloadManager> download_manager_;

 private:
  friend class base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>;

  struct ContinueFilenameDeterminationInfo;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callback function after url is checked with safebrowsing service.
  void CheckDownloadUrlDone(
      int32 download_id,
      const content::DownloadTargetCallback& callback,
      safe_browsing::DownloadProtectionService::DownloadCheckResult result);

  // Callback function after the DownloadProtectionService completes.
  void CheckClientDownloadDone(
      int32 download_id,
      safe_browsing::DownloadProtectionService::DownloadCheckResult result);

  // Callback function after we check whether the referrer URL has been visited
  // before today. Determines the danger state of the download based on the file
  // type and |visited_referrer_before|. Generates a target path for the
  // download. Invokes |DownloadPathReservationTracker::GetReservedPath| to get
  // a reserved path for the download. The path is then passed into
  // OnPathReservationAvailable().
  void CheckVisitedReferrerBeforeDone(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    content::DownloadDangerType danger_type,
    bool visited_referrer_before);

#if defined (OS_CHROMEOS)
  // DriveDownloadObserver::SubstituteDriveDownloadPath callback. Calls
  // |DownloadPathReservationTracker::GetReservedPath| to get a reserved path
  // for the download. The path is then passed into
  // OnPathReservationAvailable().
  void SubstituteDriveDownloadPathCallback(
      int32 download_id,
      const content::DownloadTargetCallback& callback,
      bool should_prompt,
      bool is_forced_path,
      content::DownloadDangerType danger_type,
      const base::FilePath& unverified_path);
#endif

  // Called on the UI thread once a reserved path is available. Updates the
  // download identified by |download_id| with the |target_path|, target
  // disposition and |danger_type|.
  void OnPathReservationAvailable(
      int32 download_id,
      const content::DownloadTargetCallback& callback,
      bool should_prompt,
      content::DownloadDangerType danger_type,
      const base::FilePath& reserved_path,
      bool reserved_path_verified);

  // When an extension opts to change a download's target filename, this
  // sanitizes it before continuing with the filename determination process.
  void OnExtensionOverridingFilename(
      const ContinueFilenameDeterminationInfo& continue_info,
      const base::FilePath& changed_filename,
      bool overwrite);

  // When extensions either opt not to change a download's target filename, or
  // the changed filename has been sanitized, this method continues with the
  // filename determination process, optionally prompting the user to manually
  // set the filename.
  void ContinueDeterminingFilename(
      const ContinueFilenameDeterminationInfo& continue_info,
      const base::FilePath& suggested_path,
      bool is_forced_path);

  // Called on the UI thread once the final target path is available.
  void OnTargetPathDetermined(
      int32 download_id,
      const content::DownloadTargetCallback& callback,
      content::DownloadItem::TargetDisposition disposition,
      content::DownloadDangerType danger_type,
      const base::FilePath& target_path);

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

  // Maps from pending extension installations to DownloadItem IDs.
  typedef base::hash_map<extensions::CrxInstaller*,
      content::DownloadOpenDelayedCallback> CrxInstallerMap;
  CrxInstallerMap crx_installers_;

  CancelableRequestConsumer history_consumer_;

  content::NotificationRegistrar registrar_;

  // The directory most recently chosen by the user in response to a Save As
  // dialog for a regular download.
  base::FilePath last_download_path_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
