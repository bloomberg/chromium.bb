// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_completion_blocker.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_file_picker.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/features.h"
#include "chrome/common/pdf_uma.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "extensions/features/features.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ppapi/features/features.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/chrome_duplicate_download_infobar_delegate.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/constants.h"
#endif

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using safe_browsing::DownloadFileType;
using safe_browsing::DownloadProtectionService;

namespace {

// The first id assigned to a download when download database failed to
// initialize.
const uint32_t kFirstDownloadIdNoPersist =
    content::DownloadItem::kInvalidId + 1;

#if defined(FULL_SAFE_BROWSING)

// String pointer used for identifying safebrowing data associated with
// a download item.
const char kSafeBrowsingUserDataKey[] = "Safe Browsing ID";

// The state of a safebrowsing check.
class SafeBrowsingState : public DownloadCompletionBlocker {
 public:
  SafeBrowsingState() {}
  ~SafeBrowsingState() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingState);
};

SafeBrowsingState::~SafeBrowsingState() {}

#endif  // FULL_SAFE_BROWSING

// Used with GetPlatformDownloadPath() to indicate which platform path to
// return.
enum PlatformDownloadPathType {
  // Return the platform specific target path.
  PLATFORM_TARGET_PATH,

  // Return the platform specific current path. If the download is in-progress
  // and the download location is a local filesystem path, then
  // GetPlatformDownloadPath will return the path to the intermediate file.
  PLATFORM_CURRENT_PATH
};

// Returns a path in the form that that is expected by platform_util::OpenItem /
// platform_util::ShowItemInFolder / DownloadTargetDeterminer.
//
// DownloadItems corresponding to Drive downloads use a temporary file as the
// target path. The paths returned by DownloadItem::GetFullPath() /
// GetTargetFilePath() refer to this temporary file. This function looks up the
// corresponding path in Drive for these downloads.
//
// How the platform path is determined is based on PlatformDownloadPathType.
base::FilePath GetPlatformDownloadPath(Profile* profile,
                                       const DownloadItem* download,
                                       PlatformDownloadPathType path_type) {
#if defined(OS_CHROMEOS)
  // Drive downloads always return the target path for all types.
  drive::DownloadHandler* drive_download_handler =
      drive::DownloadHandler::GetForProfile(profile);
  if (drive_download_handler &&
      drive_download_handler->IsDriveDownload(download))
    return drive_download_handler->GetTargetPath(download);
#endif

  if (path_type == PLATFORM_TARGET_PATH)
    return download->GetTargetFilePath();
  return download->GetFullPath();
}

#if defined(FULL_SAFE_BROWSING)
// Callback invoked by DownloadProtectionService::CheckClientDownload.
// |is_content_check_supported| is true if the SB service supports scanning the
// download for malicious content.
// |callback| is invoked with a danger type determined as follows:
//
// Danger type is (in order of preference):
//   * DANGEROUS_URL, if the URL is a known malware site.
//   * MAYBE_DANGEROUS_CONTENT, if the content will be scanned for
//         malware. I.e. |is_content_check_supported| is true.
//   * NOT_DANGEROUS.
void CheckDownloadUrlDone(
    const DownloadTargetDeterminerDelegate::CheckDownloadUrlCallback& callback,
    bool is_content_check_supported,
    DownloadProtectionService::DownloadCheckResult result) {
  content::DownloadDangerType danger_type;
  if (result == DownloadProtectionService::SAFE ||
      result == DownloadProtectionService::UNKNOWN) {
    // If this type of files is handled by the enhanced SafeBrowsing download
    // protection, mark it as potentially dangerous content until we are done
    // with scanning it.
    if (is_content_check_supported)
      danger_type = content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    else
      danger_type = content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  } else {
    // If the URL is malicious, we'll use that as the danger type. The results
    // of the content check, if one is performed, will be ignored.
    danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
  }
  callback.Run(danger_type);
}

#endif  // FULL_SAFE_BROWSING

// Called asynchronously to determine the MIME type for |path|.
std::string GetMimeType(const base::FilePath& path) {
  std::string mime_type;
  net::GetMimeTypeFromFile(path, &mime_type);
  return mime_type;
}

// Reason for why danger type is DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE.
// Used by "Download.DangerousFile.Reason" UMA metric.
// Do not change the ordering or remove items.
enum DangerousFileReason {
  SB_NOT_AVAILABLE = 0,
  SB_RETURNS_UNKOWN = 1,
  SB_RETURNS_SAFE = 2,
  DANGEROUS_FILE_REASON_MAX
};

// On Android, Chrome wants to warn the user of file overwrites rather than
// uniquify.
#if defined(OS_ANDROID)
const DownloadPathReservationTracker::FilenameConflictAction
    kDefaultPlatformConflictAction = DownloadPathReservationTracker::PROMPT;
#else
const DownloadPathReservationTracker::FilenameConflictAction
    kDefaultPlatformConflictAction = DownloadPathReservationTracker::UNIQUIFY;
#endif

}  // namespace

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate(Profile* profile)
    : profile_(profile),
      next_download_id_(content::DownloadItem::kInvalidId),
      download_prefs_(new DownloadPrefs(profile)),
      check_for_file_existence_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {}

ChromeDownloadManagerDelegate::~ChromeDownloadManagerDelegate() {
  // If a DownloadManager was set for this, Shutdown() must be called.
  DCHECK(!download_manager_);
}

void ChromeDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  download_manager_ = dm;

  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && !profile_->IsOffTheRecord()) {
    // Include this download manager in the set monitored by safe browsing.
    sb_service->AddDownloadManager(dm);
  }
}

void ChromeDownloadManagerDelegate::Shutdown() {
  download_prefs_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  download_manager_ = NULL;
}

content::DownloadIdCallback
ChromeDownloadManagerDelegate::GetDownloadIdReceiverCallback() {
  return base::Bind(&ChromeDownloadManagerDelegate::SetNextId,
                    weak_ptr_factory_.GetWeakPtr());
}

void ChromeDownloadManagerDelegate::SetNextId(uint32_t next_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_->IsOffTheRecord());

  // |content::DownloadItem::kInvalidId| will be returned only when download
  // database failed to initialize.
  bool download_db_available = (next_id != content::DownloadItem::kInvalidId);
  RecordDatabaseAvailability(download_db_available);
  if (download_db_available) {
    next_download_id_ = next_id;
  } else {
    // Still download files without download database, all download history in
    // this browser session will not be persisted.
    next_download_id_ = kFirstDownloadIdNoPersist;
  }

  IdCallbackVector callbacks;
  id_callbacks_.swap(callbacks);
  for (IdCallbackVector::const_iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    ReturnNextId(*it);
  }
}

void ChromeDownloadManagerDelegate::GetNextId(
    const content::DownloadIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (profile_->IsOffTheRecord()) {
    content::BrowserContext::GetDownloadManager(
        profile_->GetOriginalProfile())->GetDelegate()->GetNextId(callback);
    return;
  }
  if (next_download_id_ == content::DownloadItem::kInvalidId) {
    id_callbacks_.push_back(callback);
    return;
  }
  ReturnNextId(callback);
}

void ChromeDownloadManagerDelegate::ReturnNextId(
    const content::DownloadIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK_NE(content::DownloadItem::kInvalidId, next_download_id_);
  callback.Run(next_download_id_++);
}

bool ChromeDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* download,
    const content::DownloadTargetCallback& callback) {
  if (download->GetTargetFilePath().empty() &&
      download->GetMimeType() == kPDFMimeType && !download->HasUserGesture()) {
    ReportPDFLoadStatus(PDFLoadStatus::kTriggeredNoGestureDriveByDownload);
  }

  DownloadTargetDeterminer::CompletionCallback target_determined_callback =
      base::Bind(&ChromeDownloadManagerDelegate::OnDownloadTargetDetermined,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId(),
                 callback);
  DownloadTargetDeterminer::Start(
      download,
      GetPlatformDownloadPath(profile_, download, PLATFORM_TARGET_PATH),
      kDefaultPlatformConflictAction, download_prefs_.get(), this,
      target_determined_callback);
  return true;
}

bool ChromeDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (path.Extension().empty())
    return false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(asanka): This determination is done based on |path|, while
  // ShouldOpenDownload() detects extension downloads based on the
  // characteristics of the download. Reconcile this. http://crbug.com/167702
  if (path.MatchesExtension(extensions::kExtensionFileExtension))
    return false;
#endif
  return download_prefs_->IsAutoOpenEnabledBasedOnExtension(path);
}

// static
void ChromeDownloadManagerDelegate::DisableSafeBrowsing(DownloadItem* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(FULL_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&kSafeBrowsingUserDataKey));
  if (!state) {
    state = new SafeBrowsingState();
    item->SetUserData(&kSafeBrowsingUserDataKey, base::WrapUnique(state));
  }
  state->CompleteDownload();
#endif
}

bool ChromeDownloadManagerDelegate::IsDownloadReadyForCompletion(
    DownloadItem* item,
    const base::Closure& internal_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(FULL_SAFE_BROWSING)
  if (!download_prefs_->safebrowsing_for_trusted_sources_enabled() &&
      download_prefs_->IsFromTrustedSource(*item)) {
    return true;
  }

  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&kSafeBrowsingUserDataKey));
  if (!state) {
    // Begin the safe browsing download protection check.
    DownloadProtectionService* service = GetDownloadProtectionService();
    if (service) {
      DVLOG(2) << __func__ << "() Start SB download check for download = "
               << item->DebugString(false);
      state = new SafeBrowsingState();
      state->set_callback(internal_complete_callback);
      item->SetUserData(&kSafeBrowsingUserDataKey, base::WrapUnique(state));
      service->CheckClientDownload(
          item,
          base::Bind(&ChromeDownloadManagerDelegate::CheckClientDownloadDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     item->GetId()));
      return false;
    }

    // In case the service was disabled between the download starting and now,
    // we need to restore the danger state.
    content::DownloadDangerType danger_type = item->GetDangerType();
    if (DownloadItemModel(item).GetDangerLevel() !=
            DownloadFileType::NOT_DANGEROUS &&
        (danger_type == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
         danger_type ==
             content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)) {
      DVLOG(2) << __func__
               << "() SB service disabled. Marking download as DANGEROUS FILE";
      if (ShouldBlockFile(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE)) {
        item->OnContentCheckCompleted(
            // Specifying a dangerous type here would take precendence over the
            // blocking of the file.
            content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
      } else {
        item->OnContentCheckCompleted(
            content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            content::DOWNLOAD_INTERRUPT_REASON_NONE);
      }
      UMA_HISTOGRAM_ENUMERATION("Download.DangerousFile.Reason",
                                SB_NOT_AVAILABLE, DANGEROUS_FILE_REASON_MAX);
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       internal_complete_callback);
      return false;
    }
  } else if (!state->is_complete()) {
    // Don't complete the download until we have an answer.
    state->set_callback(internal_complete_callback);
    return false;
  }

#endif
  return true;
}

void ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal(
    uint32_t download_id,
    const base::Closure& user_complete_callback) {
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item)
    return;
  if (ShouldCompleteDownload(item, user_complete_callback))
    user_complete_callback.Run();
}

bool ChromeDownloadManagerDelegate::ShouldCompleteDownload(
    DownloadItem* item,
    const base::Closure& user_complete_callback) {
  return IsDownloadReadyForCompletion(item, base::Bind(
      &ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal,
      weak_ptr_factory_.GetWeakPtr(), item->GetId(), user_complete_callback));
}

bool ChromeDownloadManagerDelegate::ShouldOpenDownload(
    DownloadItem* item, const content::DownloadOpenDelayedCallback& callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (download_crx_util::IsExtensionDownload(*item) &&
      !extensions::WebstoreInstaller::GetAssociatedApproval(*item)) {
    scoped_refptr<extensions::CrxInstaller> crx_installer =
        download_crx_util::OpenChromeExtension(profile_, *item);

    // CRX_INSTALLER_DONE will fire when the install completes.  At that
    // time, Observe() will call the passed callback.
    registrar_.Add(
        this,
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(crx_installer.get()));

    crx_installers_[crx_installer.get()] = callback;
    // The status text and percent complete indicator will change now
    // that we are installing a CRX.  Update observers so that they pick
    // up the change.
    item->UpdateObservers();
    return false;
  }
#endif

  return true;
}

bool ChromeDownloadManagerDelegate::GenerateFileHash() {
#if defined(FULL_SAFE_BROWSING)
  return profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_service()->DownloadBinHashNeeded();
#else
  return false;
#endif
}

void ChromeDownloadManagerDelegate::GetSaveDir(
    content::BrowserContext* browser_context,
    base::FilePath* website_save_dir,
    base::FilePath* download_save_dir,
    bool* skip_dir_check) {
  *website_save_dir = download_prefs_->SaveFilePath();
  DCHECK(!website_save_dir->empty());
  *download_save_dir = download_prefs_->DownloadPath();
  *skip_dir_check = false;
#if defined(OS_CHROMEOS)
  *skip_dir_check = drive::util::IsUnderDriveMountPoint(*website_save_dir);
#endif
}

void ChromeDownloadManagerDelegate::ChooseSavePath(
    content::WebContents* web_contents,
    const base::FilePath& suggested_path,
    const base::FilePath::StringType& default_extension,
    bool can_save_as_complete,
    const content::SavePackagePathPickedCallback& callback) {
  // Deletes itself.
  new SavePackageFilePicker(
      web_contents,
      suggested_path,
      default_extension,
      can_save_as_complete,
      download_prefs_.get(),
      callback);
}

void ChromeDownloadManagerDelegate::SanitizeSavePackageResourceName(
    base::FilePath* filename) {
  safe_browsing::FileTypePolicies* file_type_policies =
      safe_browsing::FileTypePolicies::GetInstance();

  if (file_type_policies->GetFileDangerLevel(*filename) ==
      safe_browsing::DownloadFileType::NOT_DANGEROUS)
    return;

  base::FilePath default_filename = base::FilePath::FromUTF8Unsafe(
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
  *filename = filename->AddExtension(default_filename.BaseName().value());
}

void ChromeDownloadManagerDelegate::OpenDownloadUsingPlatformHandler(
    DownloadItem* download) {
  base::FilePath platform_path(
      GetPlatformDownloadPath(profile_, download, PLATFORM_TARGET_PATH));
  DCHECK(!platform_path.empty());
  platform_util::OpenItem(profile_, platform_path, platform_util::OPEN_FILE,
                          platform_util::OpenOperationCallback());
}

void ChromeDownloadManagerDelegate::OpenDownload(DownloadItem* download) {
  DCHECK_EQ(DownloadItem::COMPLETE, download->GetState());
  DCHECK(!download->GetTargetFilePath().empty());
  if (!download->CanOpenDownload())
    return;

  if (!DownloadItemModel(download).ShouldPreferOpeningInBrowser()) {
    RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_DEFAULT_PLATFORM);
    OpenDownloadUsingPlatformHandler(download);
    return;
  }

#if !defined(OS_ANDROID)
  content::WebContents* web_contents = download->GetWebContents();
  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : NULL;
  std::unique_ptr<chrome::ScopedTabbedBrowserDisplayer> browser_displayer;
  if (!browser ||
      !browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    browser_displayer.reset(new chrome::ScopedTabbedBrowserDisplayer(profile_));
    browser = browser_displayer->browser();
  }
  content::OpenURLParams params(
      net::FilePathToFileURL(download->GetTargetFilePath()),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);

  if (download->GetMimeType() == "application/x-x509-user-cert")
    chrome::ShowSettingsSubPage(browser, "certificates");
  else
    browser->OpenURL(params);

  RecordDownloadOpenMethod(DOWNLOAD_OPEN_METHOD_DEFAULT_BROWSER);
#else
  // ShouldPreferOpeningInBrowser() should never be true on Android.
  NOTREACHED();
#endif
}

void ChromeDownloadManagerDelegate::ShowDownloadInShell(
    DownloadItem* download) {
  if (!download->CanShowInFolder())
    return;
  base::FilePath platform_path(
      GetPlatformDownloadPath(profile_, download, PLATFORM_CURRENT_PATH));
  DCHECK(!platform_path.empty());
  platform_util::ShowItemInFolder(profile_, platform_path);
}

void ContinueCheckingForFileExistence(
    content::CheckForFileExistenceCallback callback) {
  std::move(callback).Run(false);
}

void ChromeDownloadManagerDelegate::CheckForFileExistence(
    DownloadItem* download,
    content::CheckForFileExistenceCallback callback) {
#if defined(OS_CHROMEOS)
  drive::DownloadHandler* drive_download_handler =
      drive::DownloadHandler::GetForProfile(profile_);
  if (drive_download_handler &&
      drive_download_handler->IsDriveDownload(download)) {
    drive_download_handler->CheckForFileExistence(download,
                                                  std::move(callback));
    return;
  }
#endif
  base::PostTaskAndReplyWithResult(
      check_for_file_existence_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::PathExists, download->GetTargetFilePath()),
      std::move(callback));
}

std::string
ChromeDownloadManagerDelegate::ApplicationClientIdForFileScanning() const {
  return std::string(chrome::kApplicationClientIDStringForAVScanning);
}

DownloadProtectionService*
    ChromeDownloadManagerDelegate::GetDownloadProtectionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service() &&
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return sb_service->download_protection_service();
  }
#endif
  return NULL;
}

void ChromeDownloadManagerDelegate::NotifyExtensions(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    const NotifyExtensionsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionDownloadsEventRouter* router =
      DownloadCoreServiceFactory::GetForBrowserContext(profile_)
          ->GetExtensionEventRouter();
  if (router) {
    base::Closure original_path_callback =
        base::Bind(callback, base::FilePath(),
                   DownloadPathReservationTracker::UNIQUIFY);
    router->OnDeterminingFilename(download, virtual_path.BaseName(),
                                  original_path_callback,
                                  callback);
    return;
  }
#endif
  callback.Run(base::FilePath(), DownloadPathReservationTracker::UNIQUIFY);
}

void ChromeDownloadManagerDelegate::ReserveVirtualPath(
    content::DownloadItem* download,
    const base::FilePath& virtual_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    const DownloadTargetDeterminerDelegate::ReservedPathCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path.empty());
#if defined(OS_CHROMEOS)
  if (drive::util::IsUnderDriveMountPoint(virtual_path)) {
    callback.Run(PathValidationResult::SUCCESS, virtual_path);
    return;
  }
#endif
  DownloadPathReservationTracker::GetReservedPath(
      download,
      virtual_path,
      download_prefs_->DownloadPath(),
      create_directory,
      conflict_action,
      callback);
}

void ChromeDownloadManagerDelegate::RequestConfirmation(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    DownloadConfirmationReason reason,
    const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  switch (reason) {
    case DownloadConfirmationReason::NONE:
      NOTREACHED();
      return;

    case DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE:
      DownloadManagerService::OnDownloadCanceled(
          download, DownloadController::CANCEL_REASON_NO_EXTERNAL_STORAGE);
      callback.Run(DownloadConfirmationResult::CANCELED, base::FilePath());
      return;

    case DownloadConfirmationReason::NAME_TOO_LONG:
    case DownloadConfirmationReason::TARGET_NO_SPACE:
    // These are errors. But rather than cancel the download we are going to
    // continue with the current path so that the download will get
    // interrupted again.
    //
    // Ideally we'd allow the user to try another location, but on Android,
    // the user doesn't have much of a choice (currently). So we skip the
    // prompt and try the same location.

    case DownloadConfirmationReason::SAVE_AS:
    case DownloadConfirmationReason::PREFERENCE:
      callback.Run(DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
                   suggested_path);
      return;

    case DownloadConfirmationReason::TARGET_CONFLICT:
      if (download->GetWebContents()) {
        chrome::android::ChromeDuplicateDownloadInfoBarDelegate::Create(
            InfoBarService::FromWebContents(download->GetWebContents()),
            download, suggested_path, callback);
        return;
      }
    // Fallthrough

    // If we cannot reserve the path and the WebContent is already gone, there
    // is no way to prompt user for an infobar. This could happen after chrome
    // gets killed, and user tries to resume a download while another app has
    // created the target file (not the temporary .crdownload file).
    case DownloadConfirmationReason::UNEXPECTED:
      DownloadManagerService::OnDownloadCanceled(
          download,
          DownloadController::CANCEL_REASON_CANNOT_DETERMINE_DOWNLOAD_TARGET);
      callback.Run(DownloadConfirmationResult::CANCELED, base::FilePath());
      return;
  }
#else   // !OS_ANDROID
  // Desktop Chrome displays a file picker for all confirmation needs. We can do
  // better.
  DownloadFilePicker::ShowFilePicker(download, suggested_path, callback);
#endif  // !OS_ANDROID
}

void ChromeDownloadManagerDelegate::DetermineLocalPath(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    const DownloadTargetDeterminerDelegate::LocalPathCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_CHROMEOS)
  drive::DownloadHandler* drive_download_handler =
      drive::DownloadHandler::GetForProfile(profile_);
  if (drive_download_handler) {
    drive_download_handler->SubstituteDriveDownloadPath(
        virtual_path, download, callback);
    return;
  }
#endif
  callback.Run(virtual_path);
}

void ChromeDownloadManagerDelegate::CheckDownloadUrl(
    DownloadItem* download,
    const base::FilePath& suggested_path,
    const CheckDownloadUrlCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(FULL_SAFE_BROWSING)
  safe_browsing::DownloadProtectionService* service =
      GetDownloadProtectionService();
  if (service) {
    bool is_content_check_supported =
        service->IsSupportedDownload(*download, suggested_path);
    DVLOG(2) << __func__ << "() Start SB URL check for download = "
             << download->DebugString(false);
    service->CheckDownloadUrl(download,
                              base::Bind(&CheckDownloadUrlDone, callback,
                                         is_content_check_supported));
    return;
  }
#endif
  callback.Run(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

void ChromeDownloadManagerDelegate::GetFileMimeType(
    const base::FilePath& path,
    const GetFileMimeTypeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::Bind(&GetMimeType, path), callback);
}

#if defined(FULL_SAFE_BROWSING)
void ChromeDownloadManagerDelegate::CheckClientDownloadDone(
    uint32_t download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != DownloadItem::IN_PROGRESS))
    return;

  DVLOG(2) << __func__ << "() download = " << item->DebugString(false)
           << " verdict = " << result;
  // We only mark the content as being dangerous if the download's safety state
  // has not been set to DANGEROUS yet.  We don't want to show two warnings.
  if (item->GetDangerType() == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      item->GetDangerType() ==
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT) {
    content::DownloadDangerType danger_type =
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    switch (result) {
      case DownloadProtectionService::UNKNOWN:
        // The check failed or was inconclusive.
        if (DownloadItemModel(item).GetDangerLevel() !=
            DownloadFileType::NOT_DANGEROUS) {
          danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
          UMA_HISTOGRAM_ENUMERATION("Download.DangerousFile.Reason",
                                    SB_RETURNS_UNKOWN,
                                    DANGEROUS_FILE_REASON_MAX);
        }
        break;
      case DownloadProtectionService::SAFE:
        // If this file type require explicit consent, then set the danger type
        // to DANGEROUS_FILE so that the user be required to manually vet
        // whether the download is intended or not.
        if (DownloadItemModel(item).GetDangerLevel() ==
            DownloadFileType::DANGEROUS) {
          danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
          UMA_HISTOGRAM_ENUMERATION("Download.DangerousFile.Reason",
                                    SB_RETURNS_SAFE, DANGEROUS_FILE_REASON_MAX);
        }
        break;
      case DownloadProtectionService::DANGEROUS:
        danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
        break;
      case DownloadProtectionService::UNCOMMON:
        danger_type = content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
        break;
      case DownloadProtectionService::DANGEROUS_HOST:
        danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
        break;
      case DownloadProtectionService::POTENTIALLY_UNWANTED:
        danger_type = content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
        break;
    }
    DCHECK_NE(danger_type,
              content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT);

    if (danger_type != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
      if (ShouldBlockFile(danger_type)) {
        item->OnContentCheckCompleted(
            // Specifying a dangerous type here would take precendence over the
            // blocking of the file.
            content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
      } else {
        item->OnContentCheckCompleted(danger_type,
                                      content::DOWNLOAD_INTERRUPT_REASON_NONE);
      }
    }
  }

  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&kSafeBrowsingUserDataKey));
  state->CompleteDownload();
}
#endif  // FULL_SAFE_BROWSING

// content::NotificationObserver implementation.
void ChromeDownloadManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK_EQ(extensions::NOTIFICATION_CRX_INSTALLER_DONE, type);

  registrar_.Remove(this, extensions::NOTIFICATION_CRX_INSTALLER_DONE, source);

  scoped_refptr<extensions::CrxInstaller> installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  content::DownloadOpenDelayedCallback callback =
      crx_installers_[installer.get()];
  crx_installers_.erase(installer.get());
  callback.Run(installer->did_handle_successfully());
#endif
}

void ChromeDownloadManagerDelegate::OnDownloadTargetDetermined(
    int32_t download_id,
    const content::DownloadTargetCallback& callback,
    std::unique_ptr<DownloadTargetInfo> target_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (item) {
    if (!target_info->target_path.empty() &&
        IsOpenInBrowserPreferreredForFile(target_info->target_path) &&
        target_info->is_filetype_handled_safely)
      DownloadItemModel(item).SetShouldPreferOpeningInBrowser(true);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    if (item->GetOriginalMimeType() == "application/x-x509-user-cert")
      DownloadItemModel(item).SetShouldPreferOpeningInBrowser(true);
#endif

    DownloadItemModel(item).SetDangerLevel(target_info->danger_level);
  }
  if (ShouldBlockFile(target_info->danger_type)) {
    target_info->result = content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED;
    // A dangerous type would take precendence over the blocking of the file.
    target_info->danger_type = content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  }

  callback.Run(target_info->target_path, target_info->target_disposition,
               target_info->danger_type, target_info->intermediate_path,
               target_info->result);
}

bool ChromeDownloadManagerDelegate::IsOpenInBrowserPreferreredForFile(
    const base::FilePath& path) {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  if (path.MatchesExtension(FILE_PATH_LITERAL(".pdf"))) {
    return !download_prefs_->ShouldOpenPdfInSystemReader();
  }
#endif

  // On Android, always prefer opening with an external app. On ChromeOS, there
  // are no external apps so just allow all opens to be handled by the "System."
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_PLUGINS)
  // TODO(asanka): Consider other file types and MIME types.
  // http://crbug.com/323561
  if (path.MatchesExtension(FILE_PATH_LITERAL(".pdf")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".htm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".html")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".shtm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".shtml")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".svg")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xht")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xhtm")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xhtml")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xsl")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".xslt"))) {
    return true;
  }
#endif
  return false;
}

bool ChromeDownloadManagerDelegate::ShouldBlockFile(
    content::DownloadDangerType danger_type) const {
  DownloadPrefs::DownloadRestriction download_restriction =
      download_prefs_->download_restriction();

  switch (download_restriction) {
    case (DownloadPrefs::DownloadRestriction::NONE):
      return false;

    case (DownloadPrefs::DownloadRestriction::POTENTIALLY_DANGEROUS_FILES):
      return danger_type != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

    case (DownloadPrefs::DownloadRestriction::DANGEROUS_FILES): {
      return (danger_type == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
              danger_type == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
              danger_type == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
    }

    case (DownloadPrefs::DownloadRestriction::ALL_FILES):
      return true;

    default:
      LOG(ERROR) << "Invalid download restruction value: "
                 << static_cast<int>(download_restriction);
  }
  return false;
}
