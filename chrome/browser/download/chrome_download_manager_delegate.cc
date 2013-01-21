// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/prefs/public/pref_member.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_completion_blocker.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_file_picker.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_download_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/download/download_file_picker_chromeos.h"
#include "chrome/browser/download/save_package_file_picker_chromeos.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;
using safe_browsing::DownloadProtectionService;

namespace {

// String pointer used for identifying safebrowing data associated with
// a download item.
static const char safe_browsing_id[] = "Safe Browsing ID";

// String pointer used to set the local file extension to be used when a
// download is going to be dispatched with web intents.
static const FilePath::CharType kWebIntentsFileExtension[] =
    FILE_PATH_LITERAL(".webintents");

// The state of a safebrowsing check.
class SafeBrowsingState : public DownloadCompletionBlocker {
 public:
  SafeBrowsingState()
    : verdict_(DownloadProtectionService::SAFE) {
  }

  virtual ~SafeBrowsingState();

  // The verdict that we got from calling CheckClientDownload. Only valid to
  // call if |is_complete()|.
  DownloadProtectionService::DownloadCheckResult verdict() const {
    return verdict_;
  }

  void SetVerdict(DownloadProtectionService::DownloadCheckResult result) {
    verdict_ = result;
    CompleteDownload();
  }

 private:
  DownloadProtectionService::DownloadCheckResult verdict_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingState);
};

SafeBrowsingState::~SafeBrowsingState() {}

// Generate a filename based on the response from the server.  Similar
// in operation to net::GenerateFileName(), but uses a localized
// default name.
void GenerateFileNameFromRequest(const DownloadItem& download_item,
                                 FilePath* generated_name,
                                 std::string referrer_charset) {
  std::string default_file_name(
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));

  *generated_name = net::GenerateFileName(download_item.GetURL(),
                                          download_item.GetContentDisposition(),
                                          referrer_charset,
                                          download_item.GetSuggestedFilename(),
                                          download_item.GetMimeType(),
                                          default_file_name);
}

// Needed to give PostTask a void closure in OnWebIntentDispatchCompleted.
void DeleteFile(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file_util::Delete(file_path, false);
}

// Called when the web intents dispatch has completed. Deletes the |file_path|.
void OnWebIntentDispatchCompleted(
    const FilePath& file_path,
    webkit_glue::WebIntentReplyType intent_reply) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DeleteFile, file_path));
}

typedef base::Callback<void(bool)> VisitedBeforeCallback;

// Condenses the results from HistoryService::GetVisibleVisitCountToHost() to a
// single bool so that VisitedBeforeCallback can curry up to 5 other parameters
// without a struct.
void VisitCountsToVisitedBefore(
    const VisitedBeforeCallback& callback,
    HistoryService::Handle unused_handle,
    bool found_visits,
    int count,
    base::Time first_visit) {
  callback.Run(found_visits && count &&
      (first_visit.LocalMidnight() < base::Time::Now().LocalMidnight()));
}

}  // namespace

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate(Profile* profile)
    : profile_(profile),
      next_download_id_(0),
      download_prefs_(new DownloadPrefs(profile)) {
}

ChromeDownloadManagerDelegate::~ChromeDownloadManagerDelegate() {
}

void ChromeDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  download_manager_ = dm;
#if !defined(OS_ANDROID)
  extension_event_router_.reset(new ExtensionDownloadsEventRouter(
      profile_, download_manager_));
#endif
}

void ChromeDownloadManagerDelegate::Shutdown() {
  download_prefs_.reset();
#if !defined(OS_ANDROID)
  extension_event_router_.reset();
#endif
}

DownloadId ChromeDownloadManagerDelegate::GetNextId() {
  if (!profile_->IsOffTheRecord())
    return DownloadId(this, next_download_id_++);

  return BrowserContext::GetDownloadManager(profile_->GetOriginalProfile())->
      GetDelegate()->GetNextId();
}

bool ChromeDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* download,
    const content::DownloadTargetCallback& callback) {
#if defined(FULL_SAFE_BROWSING)
  DownloadProtectionService* service = GetDownloadProtectionService();
  if (service) {
    VLOG(2) << __FUNCTION__ << "() Start SB URL check for download = "
            << download->DebugString(false);
    service->CheckDownloadUrl(
        DownloadProtectionService::DownloadInfo::FromDownloadItem(*download),
        base::Bind(
            &ChromeDownloadManagerDelegate::CheckDownloadUrlDone,
            this,
            download->GetId(),
            callback));
    return true;
  }
#endif
  CheckDownloadUrlDone(download->GetId(), callback,
                       DownloadProtectionService::SAFE);
  return true;
}

void ChromeDownloadManagerDelegate::ChooseDownloadPath(
    DownloadItem* item,
    const FilePath& suggested_path,
    const FileSelectedCallback& file_selected_callback) {
  // Deletes itself.
  DownloadFilePicker* file_picker =
#if defined(OS_CHROMEOS)
      new DownloadFilePickerChromeOS();
#else
      new DownloadFilePicker();
#endif
  file_picker->Init(download_manager_, item, suggested_path,
                    file_selected_callback);
}

FilePath ChromeDownloadManagerDelegate::GetIntermediatePath(
    const FilePath& target_path,
    content::DownloadDangerType danger_type) {
  // If the download is not dangerous, just append .crdownload to the target
  // path.
  if (danger_type == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    return download_util::GetCrDownloadPath(target_path);

  // If the download is potentially dangerous we create a filename of the form
  // 'Unconfirmed <random>.crdownload'.
  FilePath::StringType file_name;
  FilePath dir = target_path.DirName();
#if defined(OS_WIN)
  string16 unconfirmed_prefix =
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#else
  std::string unconfirmed_prefix =
      l10n_util::GetStringUTF8(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#endif
  base::SStringPrintf(
      &file_name,
      unconfirmed_prefix.append(
          FILE_PATH_LITERAL(" %d.crdownload")).c_str(),
      base::RandInt(0, 1000000));
  return dir.Append(file_name);
}

WebContents* ChromeDownloadManagerDelegate::
    GetAlternativeWebContentsToNotifyForDownload() {
#if defined(OS_ANDROID)
  // Android does not implement BrowserList or any other way to get an
  // alternate web contents.
  return NULL;
#else
  // Start the download in the last active browser. This is not ideal but better
  // than fully hiding the download from the user.
  Browser* last_active = chrome::FindLastActiveWithProfile(profile_,
      chrome::GetActiveDesktop());
  return last_active ? chrome::GetActiveWebContents(last_active) : NULL;
#endif
}

bool ChromeDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  if (extensions::Extension::IsExtension(path))
    return false;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  return download_prefs_->IsAutoOpenEnabledForExtension(extension);
}

// static
void ChromeDownloadManagerDelegate::DisableSafeBrowsing(DownloadItem* item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(FULL_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&safe_browsing_id));
  if (!state) {
    state = new SafeBrowsingState();
    item->SetUserData(&safe_browsing_id, state);
  }
  state->SetVerdict(DownloadProtectionService::SAFE);
#endif
}

bool ChromeDownloadManagerDelegate::IsDownloadReadyForCompletion(
    DownloadItem* item,
    const base::Closure& internal_complete_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(FULL_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&safe_browsing_id));
  if (!state) {
    // Begin the safe browsing download protection check.
    DownloadProtectionService* service = GetDownloadProtectionService();
    if (service) {
      VLOG(2) << __FUNCTION__ << "() Start SB download check for download = "
              << item->DebugString(false);
      state = new SafeBrowsingState();
      state->set_callback(internal_complete_callback);
      item->SetUserData(&safe_browsing_id, state);
      service->CheckClientDownload(
          DownloadProtectionService::DownloadInfo::FromDownloadItem(*item),
          base::Bind(
              &ChromeDownloadManagerDelegate::CheckClientDownloadDone,
              this,
              item->GetId()));
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
    int download_id,
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
      this, item->GetId(), user_complete_callback));
}

bool ChromeDownloadManagerDelegate::ShouldOpenDownload(
    DownloadItem* item, const content::DownloadOpenDelayedCallback& callback) {
  if (download_crx_util::IsExtensionDownload(*item)) {
    scoped_refptr<extensions::CrxInstaller> crx_installer =
        download_crx_util::OpenChromeExtension(profile_, *item);

    // CRX_INSTALLER_DONE will fire when the install completes.  At that
    // time, Observe() will call the passed callback.
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(crx_installer.get()));

    crx_installers_[crx_installer.get()] = callback;
    // The status text and percent complete indicator will change now
    // that we are installing a CRX.  Update observers so that they pick
    // up the change.
    item->UpdateObservers();
    return false;
  }

  if (ShouldOpenWithWebIntents(item)) {
    OpenWithWebIntent(item);
    callback.Run(true);
    return false;
  }

  return true;
}

bool ChromeDownloadManagerDelegate::ShouldOpenWithWebIntents(
    const DownloadItem* item) {
  if (!web_intents::IsWebIntentsEnabledForProfile(profile_))
    return false;

  if ((item->GetWebContents() && !item->GetWebContents()->GetDelegate()) &&
      !web_intents::GetBrowserForBackgroundWebIntentDelivery(profile_)) {
    return false;
  }
  if (!item->GetForcedFilePath().empty())
    return false;
  if (item->GetTargetDisposition() == DownloadItem::TARGET_DISPOSITION_PROMPT)
    return false;

#if !defined(OS_CHROMEOS)
  std::string mime_type = item->GetMimeType();

  // If QuickOffice extension is installed, and we're not on ChromeOS,use web
  // intents to handle the downloaded file.
  const char kQuickOfficeExtensionId[] = "gbkeegbaiigmenfmjfclcdgdpimamgkj";
  const char kQuickOfficeDevExtensionId[] = "ionpfmkccalenbmnddpbmocokhaknphg";
  ExtensionServiceInterface* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  bool use_quickoffice = false;
  if (extension_service &&
      extension_service->GetInstalledExtension(kQuickOfficeExtensionId) &&
      extension_service->IsExtensionEnabled(kQuickOfficeExtensionId))
    use_quickoffice = true;

  if (extension_service &&
      extension_service->GetInstalledExtension(kQuickOfficeDevExtensionId) &&
      extension_service->IsExtensionEnabled(kQuickOfficeDevExtensionId))
    use_quickoffice = true;

  if (use_quickoffice) {
    if (mime_type == "application/msword" ||
        mime_type == "application/vnd.ms-powerpoint" ||
        mime_type == "application/vnd.ms-excel" ||
        mime_type == "application/vnd.openxmlformats-officedocument."
                     "wordprocessingml.document" ||
        mime_type == "application/vnd.openxmlformats-officedocument."
                     "presentationml.presentation" ||
        mime_type == "application/vnd.openxmlformats-officedocument."
                     "spreadsheetml.sheet") {
      return true;
    }
  }
#endif  // !defined(OS_CHROMEOS)

  return false;
}

void ChromeDownloadManagerDelegate::OpenWithWebIntent(
    const DownloadItem* item) {
  webkit_glue::WebIntentData intent_data(
      ASCIIToUTF16("http://webintents.org/view"),
      ASCIIToUTF16(item->GetMimeType()),
      item->GetFullPath(),
      item->GetReceivedBytes());

  // RCH specifies that the receiver gets the url, but with Web Intents
  // it isn't really needed.
  intent_data.extra_data.insert(make_pair(
      ASCIIToUTF16("url"), ASCIIToUTF16(item->GetURL().spec())));

  // Pass the downloaded filename to the service app as the name hint.
  intent_data.extra_data.insert(
      make_pair(ASCIIToUTF16("filename"),
                item->GetFileNameToReportUser().LossyDisplayName()));

  content::WebIntentsDispatcher* dispatcher =
      content::WebIntentsDispatcher::Create(intent_data);
  dispatcher->RegisterReplyNotification(
      base::Bind(&OnWebIntentDispatchCompleted, item->GetFullPath()));

  content::WebContentsDelegate* delegate = NULL;
#if !defined(OS_ANDROID)
  if (item->GetWebContents() && item->GetWebContents()->GetDelegate()) {
    delegate = item->GetWebContents()->GetDelegate();
  } else {
    delegate = web_intents::GetBrowserForBackgroundWebIntentDelivery(profile_);
  }
#endif  // !defined(OS_ANDROID)
  DCHECK(delegate);
  delegate->WebIntentDispatch(NULL, dispatcher);
}

bool ChromeDownloadManagerDelegate::GenerateFileHash() {
#if defined(FULL_SAFE_BROWSING)
  return profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_service()->DownloadBinHashNeeded();
#else
  return false;
#endif
}

void ChromeDownloadManagerDelegate::GetSaveDir(BrowserContext* browser_context,
                                               FilePath* website_save_dir,
                                               FilePath* download_save_dir,
                                               bool* skip_dir_check) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefServiceSyncable* prefs = profile->GetPrefs();

  // Check whether the preference has the preferred directory for saving file.
  // If not, initialize it with default directory.
  if (!prefs->FindPreference(prefs::kSaveFileDefaultDirectory)) {
    DCHECK(prefs->FindPreference(prefs::kDownloadDefaultDirectory));
    FilePath default_save_path = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    prefs->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                default_save_path,
                                PrefServiceSyncable::UNSYNCABLE_PREF);
  }

  // Get the directory from preference.
  *website_save_dir = prefs->GetFilePath(prefs::kSaveFileDefaultDirectory);
  DCHECK(!website_save_dir->empty());

  *download_save_dir = prefs->GetFilePath(prefs::kDownloadDefaultDirectory);

  *skip_dir_check = false;
#if defined(OS_CHROMEOS)
  *skip_dir_check = drive::util::IsUnderDriveMountPoint(*website_save_dir);
#endif
}

void ChromeDownloadManagerDelegate::ChooseSavePath(
    WebContents* web_contents,
    const FilePath& suggested_path,
    const FilePath::StringType& default_extension,
    bool can_save_as_complete,
    const content::SavePackagePathPickedCallback& callback) {
  // Deletes itself.
#if defined(OS_CHROMEOS)
  new SavePackageFilePickerChromeOS(web_contents, suggested_path, callback);
#else
  new SavePackageFilePicker(web_contents, suggested_path, default_extension,
      can_save_as_complete, download_prefs_.get(), callback);
#endif
}

void ChromeDownloadManagerDelegate::OpenDownload(DownloadItem* download) {
  platform_util::OpenItem(download->GetFullPath());
}

void ChromeDownloadManagerDelegate::ShowDownloadInShell(
    DownloadItem* download) {
  platform_util::ShowItemInFolder(download->GetFullPath());
}

void ChromeDownloadManagerDelegate::ClearLastDownloadPath() {
  last_download_path_.clear();
}

DownloadProtectionService*
    ChromeDownloadManagerDelegate::GetDownloadProtectionService() {
#if defined(FULL_SAFE_BROWSING)
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service() &&
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return sb_service->download_protection_service();
  }
#endif
  return NULL;
}

// TODO(phajdan.jr): This is apparently not being exercised in tests.
bool ChromeDownloadManagerDelegate::IsDangerousFile(
    const DownloadItem& download,
    const FilePath& suggested_path,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Anything loaded directly from the address bar is OK.
  if (download.GetTransitionType() & content::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    return false;

  // Extensions that are not from the gallery are considered dangerous.
  // When off-store install is disabled we skip this, since in this case, we
  // will not offer to install the extension.
  if (extensions::FeatureSwitch::easy_off_store_install()->IsEnabled() &&
      download_crx_util::IsExtensionDownload(download) &&
      !extensions::WebstoreInstaller::GetAssociatedApproval(download)) {
    return true;
  }

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (ShouldOpenFileBasedOnExtension(suggested_path) &&
      download.HasUserGesture())
    return false;

  // "Allow on user gesture" is OK when we have a user gesture and the hosting
  // page has been visited before today.
  download_util::DownloadDangerLevel danger_level =
      download_util::GetFileDangerLevel(suggested_path.BaseName());
  if (danger_level == download_util::AllowOnUserGesture)
    return !download.HasUserGesture() || !visited_referrer_before;

  return danger_level == download_util::Dangerous;
}

void ChromeDownloadManagerDelegate::GetReservedPath(
    DownloadItem& download,
    const FilePath& target_path,
    const FilePath& default_download_path,
    bool should_uniquify_path,
    const DownloadPathReservationTracker::ReservedPathCallback& callback) {
  DownloadPathReservationTracker::GetReservedPath(
      download, target_path, default_download_path, should_uniquify_path,
      callback);
}

void ChromeDownloadManagerDelegate::CheckDownloadUrlDone(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    DownloadProtectionService::DownloadCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download = download_manager_->GetDownload(download_id);
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;

  VLOG(2) << __FUNCTION__ << "() download = " << download->DebugString(false)
          << " verdict = " << result;
  content::DownloadDangerType danger_type = download->GetDangerType();
  if (result != DownloadProtectionService::SAFE)
    danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;

  // HistoryServiceFactory redirects incognito profiles to on-record profiles.
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (!history || !download->GetReferrerUrl().is_valid()) {
    // If the original profile doesn't have a HistoryService or the referrer url
    // is invalid, then give up and assume the referrer has not been visited
    // before. There's no history for on-record profiles in unit_tests, for
    // example.
    CheckVisitedReferrerBeforeDone(download_id, callback, danger_type, false);
    return;
  }
  history->GetVisibleVisitCountToHost(
      download->GetReferrerUrl(), &history_consumer_,
      base::Bind(&VisitCountsToVisitedBefore, base::Bind(
          &ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone,
          this, download_id, callback, danger_type)));
}

void ChromeDownloadManagerDelegate::CheckClientDownloadDone(
    int32 download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != DownloadItem::IN_PROGRESS))
    return;

  VLOG(2) << __FUNCTION__ << "() download = " << item->DebugString(false)
          << " verdict = " << result;
  // We only mark the content as being dangerous if the download's safety state
  // has not been set to DANGEROUS yet.  We don't want to show two warnings.
  if (item->GetDangerType() == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      item->GetDangerType() ==
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT) {
    switch (result) {
      case DownloadProtectionService::SAFE:
        // Do nothing.
        break;
      case DownloadProtectionService::DANGEROUS:
        item->OnContentCheckCompleted(
            content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
        break;
      case DownloadProtectionService::UNCOMMON:
        item->OnContentCheckCompleted(
            content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
        break;
    }
  }

  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetUserData(&safe_browsing_id));
  state->SetVerdict(result);
}

// content::NotificationObserver implementation.
void ChromeDownloadManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_CRX_INSTALLER_DONE);

  registrar_.Remove(this,
                    chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                    source);

  scoped_refptr<extensions::CrxInstaller> installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  content::DownloadOpenDelayedCallback callback = crx_installers_[installer];
  crx_installers_.erase(installer.get());
  callback.Run(installer->did_handle_successfully());
}

void ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    content::DownloadDangerType danger_type,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;

  bool should_prompt = (download->GetTargetDisposition() ==
                        DownloadItem::TARGET_DISPOSITION_PROMPT);
  bool is_forced_path = !download->GetForcedFilePath().empty();
  FilePath suggested_path;

  // Check whether this download is for an extension install or not.
  // Allow extensions to be explicitly saved.
  if (!is_forced_path) {
    FilePath generated_name;
    GenerateFileNameFromRequest(
        *download,
        &generated_name,
        profile_->GetPrefs()->GetString(prefs::kDefaultCharset));

    // Freeze the user's preference for showing a Save As dialog.  We're going
    // to bounce around a bunch of threads and we don't want to worry about race
    // conditions where the user changes this pref out from under us.
    if (download_prefs_->PromptForDownload()) {
      // But ignore the user's preference for the following scenarios:
      // 1) Extension installation. Note that we only care here about the case
      //    where an extension is installed, not when one is downloaded with
      //    "save as...".
      // 2) Filetypes marked "always open." If the user just wants this file
      //    opened, don't bother asking where to keep it.
      if (!download_crx_util::IsExtensionDownload(*download) &&
          !ShouldOpenFileBasedOnExtension(generated_name))
        should_prompt = true;
    }
    if (download_prefs_->IsDownloadPathManaged())
      should_prompt = false;

    // Determine the proper path for a download, by either one of the following:
    // 1) using the default download directory.
    // 2) prompting the user.
    FilePath target_directory;
    if (should_prompt && !last_download_path_.empty())
      target_directory = last_download_path_;
    else
      target_directory = download_prefs_->DownloadPath();
    suggested_path = target_directory.Append(generated_name);
  } else {
    DCHECK(!should_prompt);
    suggested_path = download->GetForcedFilePath();
  }

  // If we will open the file with a web intents dispatch,
  // give it a name that will not allow the OS to open it using usual
  // associated apps.
  if (ShouldOpenWithWebIntents(download)) {
    download->SetDisplayName(suggested_path.BaseName());
    suggested_path = suggested_path.AddExtension(kWebIntentsFileExtension);
  }

  // If the download hasn't already been marked dangerous (could be
  // DANGEROUS_URL), check if it is a dangerous file.
  if (danger_type == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    if (!should_prompt && !is_forced_path &&
        IsDangerousFile(*download, suggested_path, visited_referrer_before)) {
      danger_type = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    }

#if defined(FULL_SAFE_BROWSING)
    DownloadProtectionService* service = GetDownloadProtectionService();
    // If this type of files is handled by the enhanced SafeBrowsing download
    // protection, mark it as potentially dangerous content until we are done
    // with scanning it.
    if (service && service->enabled()) {
      DownloadProtectionService::DownloadInfo info =
          DownloadProtectionService::DownloadInfo::FromDownloadItem(*download);
      info.target_file = suggested_path;
      // TODO(noelutz): if the user changes the extension name in the UI to
      // something like .exe SafeBrowsing will currently *not* check if the
      // download is malicious.
      if (service->IsSupportedDownload(info))
        danger_type = content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    }
#endif
  } else {
    // Currently we only expect this case.
    DCHECK_EQ(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL, danger_type);
  }

#if defined (OS_CHROMEOS)
  drive::DriveDownloadObserver::SubstituteDriveDownloadPath(
      profile_, suggested_path, download,
      base::Bind(
          &ChromeDownloadManagerDelegate::SubstituteDriveDownloadPathCallback,
          this, download->GetId(), callback, should_prompt, is_forced_path,
          danger_type));
#else
  GetReservedPath(
      *download, suggested_path, download_prefs_->DownloadPath(),
      !is_forced_path,
      base::Bind(&ChromeDownloadManagerDelegate::OnPathReservationAvailable,
                 this, download->GetId(), callback, should_prompt,
                 danger_type));
#endif
}

#if defined (OS_CHROMEOS)
// TODO(asanka): Merge this logic with the logic in DownloadFilePickerChromeOS.
void ChromeDownloadManagerDelegate::SubstituteDriveDownloadPathCallback(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    bool should_prompt,
    bool is_forced_path,
    content::DownloadDangerType danger_type,
    const FilePath& suggested_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;

  GetReservedPath(
      *download, suggested_path, download_prefs_->DownloadPath(),
      !is_forced_path,
      base::Bind(&ChromeDownloadManagerDelegate::OnPathReservationAvailable,
                 this, download->GetId(), callback, should_prompt,
                 danger_type));
}
#endif

void ChromeDownloadManagerDelegate::OnPathReservationAvailable(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    bool should_prompt,
    content::DownloadDangerType danger_type,
    const FilePath& reserved_path,
    bool reserved_path_verified) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;
  if (should_prompt || !reserved_path_verified) {
    // If the target path could not be verified then the path was non-existant,
    // non writeable or could not be uniquified. Prompt the user.
    ChooseDownloadPath(
        download, reserved_path,
        base::Bind(&ChromeDownloadManagerDelegate::OnTargetPathDetermined,
                   this, download_id, callback,
                   DownloadItem::TARGET_DISPOSITION_PROMPT, danger_type));
  } else {
    OnTargetPathDetermined(download_id, callback,
                           DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                           danger_type, reserved_path);
  }
}

void ChromeDownloadManagerDelegate::OnTargetPathDetermined(
    int32 download_id,
    const content::DownloadTargetCallback& callback,
    DownloadItem::TargetDisposition disposition,
    content::DownloadDangerType danger_type,
    const FilePath& target_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath intermediate_path;
  DownloadItem* download =
      download_manager_->GetDownload(download_id);
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;

  // If |target_path| is empty, then that means that the user wants to cancel
  // the download.
  if (!target_path.empty()) {
    intermediate_path = GetIntermediatePath(target_path, danger_type);

    // Retain the last directory. Exclude temporary downloads since the path
    // likely points at the location of a temporary file.
    // TODO(asanka): This logic is a hack. DownloadFilePicker should give us a
    //               directory to persist. Or perhaps, if the Drive path
    //               substitution logic is moved here, then we would have a
    //               persistable path after the DownloadFilePicker is done.
    if (disposition == DownloadItem::TARGET_DISPOSITION_PROMPT &&
        !download->IsTemporary())
      last_download_path_ = target_path.DirName();
  }
  callback.Run(target_path, disposition, danger_type, intermediate_path);
}
