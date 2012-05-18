// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/path_service.h"
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
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/web_intent_data.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/download/download_file_picker_chromeos.h"
#include "chrome/browser/download/save_package_file_picker_chromeos.h"
#endif

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

}  // namespace

ChromeDownloadManagerDelegate::ChromeDownloadManagerDelegate(Profile* profile)
    : profile_(profile),
      next_download_id_(0),
      download_prefs_(new DownloadPrefs(profile->GetPrefs())) {
}

ChromeDownloadManagerDelegate::~ChromeDownloadManagerDelegate() {
}

bool ChromeDownloadManagerDelegate::IsExtensionDownload(
    const DownloadItem* item) {
  if (item->PromptUserForSaveLocation())
    return false;

  if (item->GetMimeType() == Extension::kMimeType ||
      UserScript::IsURLUserScript(item->GetURL(), item->GetMimeType())) {
    return true;
  } else {
    return false;
  }
}

void ChromeDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  download_manager_ = dm;
  download_history_.reset(new DownloadHistory(profile_));
  download_history_->Load(
      base::Bind(&DownloadManager::OnPersistentStoreQueryComplete,
                 base::Unretained(dm)));
}

void ChromeDownloadManagerDelegate::Shutdown() {
  download_history_.reset();
  download_prefs_.reset();
}

DownloadId ChromeDownloadManagerDelegate::GetNextId() {
  if (!profile_->IsOffTheRecord())
    return DownloadId(this, next_download_id_++);

  return profile_->GetOriginalProfile()->GetDownloadManager()->delegate()->
      GetNextId();
}

bool ChromeDownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  // We create a download item and store it in our download map, and inform the
  // history system of a new download. Since this method can be called while the
  // history service thread is still reading the persistent state, we do not
  // insert the new DownloadItem into 'history_downloads_' or inform our
  // observers at this point. OnCreateDownloadEntryComplete() handles that
  // finalization of the the download creation as a callback from the history
  // thread.
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return false;

#if defined(ENABLE_SAFE_BROWSING)
  DownloadProtectionService* service = GetDownloadProtectionService();
  if (service) {
    VLOG(2) << __FUNCTION__ << "() Start SB URL check for download = "
            << download->DebugString(false);
    service->CheckDownloadUrl(
        DownloadProtectionService::DownloadInfo::FromDownloadItem(*download),
        base::Bind(
            &ChromeDownloadManagerDelegate::CheckDownloadUrlDone,
            this,
            download->GetId()));
    return false;
  }
#endif
  CheckDownloadUrlDone(download_id, DownloadProtectionService::SAFE);
  return false;
}

void ChromeDownloadManagerDelegate::ChooseDownloadPath(
    WebContents* web_contents,
    const FilePath& suggested_path,
    int32 download_id) {
  // Deletes itself.
#if defined(OS_CHROMEOS)
  new DownloadFilePickerChromeOS
#else
  new DownloadFilePicker
#endif
      (download_manager_, web_contents, suggested_path, download_id);
}

FilePath ChromeDownloadManagerDelegate::GetIntermediatePath(
    const FilePath& suggested_path) {
  return download_util::GetCrDownloadPath(suggested_path);
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
  Browser* last_active = browser::FindLastActiveWithProfile(profile_);
  return last_active ? last_active->GetSelectedWebContents() : NULL;
#endif
}


bool ChromeDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  if (Extension::IsExtension(path))
    return false;
  DCHECK(extension[0] == FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  return download_prefs_->IsAutoOpenEnabledForExtension(extension);
}

// static
void ChromeDownloadManagerDelegate::DisableSafeBrowsing(DownloadItem* item) {
#if defined(ENABLE_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetExternalData(&safe_browsing_id));
  DCHECK(!state);
  if (!state)
    state = new SafeBrowsingState();
  state->SetVerdict(DownloadProtectionService::SAFE);
  item->SetExternalData(&safe_browsing_id, state);
#endif
}

bool ChromeDownloadManagerDelegate::IsDownloadReadyForCompletion(
    DownloadItem* item,
    const base::Closure& internal_complete_callback) {
#if defined(ENABLE_SAFE_BROWSING)
  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetExternalData(&safe_browsing_id));
  if (!state) {
    // Begin the safe browsing download protection check.
    DownloadProtectionService* service = GetDownloadProtectionService();
    if (service) {
      VLOG(2) << __FUNCTION__ << "() Start SB download check for download = "
              << item->DebugString(false);
      state = new SafeBrowsingState();
      state->set_callback(internal_complete_callback);
      item->SetExternalData(&safe_browsing_id, state);
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

#if defined(OS_CHROMEOS)
  // If there's a GData upload associated with this download, we wait until that
  // is complete before allowing the download item to complete.
  if (!gdata::GDataDownloadObserver::IsReadyToComplete(
        item, internal_complete_callback))
    return false;
#endif
  return true;
}

// ShouldCompleteDownloadInternal() will never be called directly by a user, it
// will only be called asynchronously, so it should run
// |user_complete_callback|. ShouldCompleteDownload() will only be called
// directly by a user, so it does not need to run |user_complete_callback|
// because it can return true synchronously. The two methods look very similar,
// but their semantics are very different.

void ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal(
    int download_id,
    const base::Closure& user_complete_callback) {
  DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
  if (!item)
    return;
  if (IsDownloadReadyForCompletion(item, base::Bind(
        &ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal, this,
        download_id, user_complete_callback)))
    user_complete_callback.Run();
}

bool ChromeDownloadManagerDelegate::ShouldCompleteDownload(
    DownloadItem* item,
    const base::Closure& user_complete_callback) {
  return IsDownloadReadyForCompletion(item, base::Bind(
      &ChromeDownloadManagerDelegate::ShouldCompleteDownloadInternal, this,
      item->GetId(), user_complete_callback));
}

bool ChromeDownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  if (IsExtensionDownload(item)) {
    // We can open extensions if either they came from the store, or
    // off-store-install is enabled.
    if (extensions::switch_utils::IsOffStoreInstallEnabled() ||
        WebstoreInstaller::GetAssociatedApproval(*item)) {
      scoped_refptr<CrxInstaller> crx_installer =
          download_crx_util::OpenChromeExtension(profile_, *item);

      // CRX_INSTALLER_DONE will fire when the install completes.  Observe()
      // will call DelayedDownloadOpened() on this item.  If this DownloadItem
      // is not around when CRX_INSTALLER_DONE fires, Complete() will not be
      // called.
      registrar_.Add(this,
                     chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(crx_installer.get()));

      crx_installers_[crx_installer.get()] = item->GetId();
      // The status text and percent complete indicator will change now
      // that we are installing a CRX.  Update observers so that they pick
      // up the change.
      item->UpdateObservers();
      return false;
    }
  }

  if (ShouldOpenWithWebIntents(item)) {
    OpenWithWebIntent(item);
    item->DelayedDownloadOpened();
    return false;
  }

  return true;
}

bool ChromeDownloadManagerDelegate::ShouldOpenWithWebIntents(
    const DownloadItem* item) {
  if (!item->GetWebContents() || !item->GetWebContents()->GetDelegate())
    return false;

  std::string mime_type = item->GetMimeType();
  if (mime_type == "application/rss+xml" ||
      mime_type == "application/atom+xml") {
    return true;
  }

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

  return false;
}

void ChromeDownloadManagerDelegate::OpenWithWebIntent(
    const DownloadItem* item) {
  webkit_glue::WebIntentData intent_data(
      ASCIIToUTF16("http://webintents.org/view"),
      ASCIIToUTF16(item->GetMimeType()),
      item->GetFullPath(),
      item->GetReceivedBytes());

  // TODO(gbillock): Should we pass this? RCH specifies that the receiver gets
  // the url, but with web intents we don't need to pass it.
  intent_data.extra_data.insert(make_pair(
      ASCIIToUTF16("url"), ASCIIToUTF16(item->GetURL().spec())));

  content::WebIntentsDispatcher* dispatcher =
      content::WebIntentsDispatcher::Create(intent_data);
  // TODO(gbillock): try to get this to be able to delegate to the Browser
  // object directly, passing a NULL WebContents?
  item->GetWebContents()->GetDelegate()->WebIntentDispatch(
      item->GetWebContents(), dispatcher);
}

bool ChromeDownloadManagerDelegate::GenerateFileHash() {
#if defined(ENABLE_SAFE_BROWSING)
  return profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_service()->DownloadBinHashNeeded();
#else
  return false;
#endif
}

void ChromeDownloadManagerDelegate::AddItemToPersistentStore(
    DownloadItem* item) {
  download_history_->AddEntry(item,
      base::Bind(&ChromeDownloadManagerDelegate::OnItemAddedToPersistentStore,
                 base::Unretained(this)));
}

void ChromeDownloadManagerDelegate::UpdateItemInPersistentStore(
    DownloadItem* item) {
  download_history_->UpdateEntry(item);
}

void ChromeDownloadManagerDelegate::UpdatePathForItemInPersistentStore(
    DownloadItem* item,
    const FilePath& new_path) {
  download_history_->UpdateDownloadPath(item, new_path);
}

void ChromeDownloadManagerDelegate::RemoveItemFromPersistentStore(
    DownloadItem* item) {
  download_history_->RemoveEntry(item);
}

void ChromeDownloadManagerDelegate::RemoveItemsFromPersistentStoreBetween(
    base::Time remove_begin,
    base::Time remove_end) {
  download_history_->RemoveEntriesBetween(remove_begin, remove_end);
}

void ChromeDownloadManagerDelegate::GetSaveDir(WebContents* web_contents,
                                               FilePath* website_save_dir,
                                               FilePath* download_save_dir) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Check whether the preference has the preferred directory for saving file.
  // If not, initialize it with default directory.
  if (!prefs->FindPreference(prefs::kSaveFileDefaultDirectory)) {
    DCHECK(prefs->FindPreference(prefs::kDownloadDefaultDirectory));
    FilePath default_save_path = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    prefs->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                default_save_path,
                                PrefService::UNSYNCABLE_PREF);
  }

  // Get the directory from preference.
  *website_save_dir = prefs->GetFilePath(prefs::kSaveFileDefaultDirectory);
  DCHECK(!website_save_dir->empty());

  *download_save_dir = prefs->GetFilePath(prefs::kDownloadDefaultDirectory);
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

#if defined(ENABLE_SAFE_BROWSING)
DownloadProtectionService*
    ChromeDownloadManagerDelegate::GetDownloadProtectionService() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  if (sb_service && sb_service->download_protection_service() &&
      profile_->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    return sb_service->download_protection_service();
  }
  return NULL;
}
#endif

void ChromeDownloadManagerDelegate::CheckDownloadUrlDone(
    int32 download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;

  VLOG(2) << __FUNCTION__ << "() download = " << download->DebugString(false)
          << " verdict = " << result;
  if (result != DownloadProtectionService::SAFE)
    download->SetDangerType(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);

  download_history_->CheckVisitedReferrerBefore(
      download_id, download->GetReferrerUrl(),
      base::Bind(&ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone,
                 base::Unretained(this)));
}

void ChromeDownloadManagerDelegate::CheckClientDownloadDone(
    int32 download_id,
    DownloadProtectionService::DownloadCheckResult result) {
  DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
  if (!item)
    return;

  VLOG(2) << __FUNCTION__ << "() download = " << item->DebugString(false)
          << " verdict = " << result;
  // We only mark the content as being dangerous if the download's safety state
  // has not been set to DANGEROUS yet.  We don't want to show two warnings.
  if (item->GetSafetyState() == DownloadItem::SAFE) {
    switch (result) {
      case DownloadProtectionService::SAFE:
        // Do nothing.
        break;
      case DownloadProtectionService::DANGEROUS:
        item->SetDangerType(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
        break;
      case DownloadProtectionService::UNCOMMON:
        item->SetDangerType(content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
        break;
    }
  }

  SafeBrowsingState* state = static_cast<SafeBrowsingState*>(
      item->GetExternalData(&safe_browsing_id));
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

  CrxInstaller* installer = content::Source<CrxInstaller>(source).ptr();
  int download_id = crx_installers_[installer];
  crx_installers_.erase(installer);

  DownloadItem* item = download_manager_->GetActiveDownloadItem(download_id);
  if (item)
    item->DelayedDownloadOpened();
}

void ChromeDownloadManagerDelegate::CheckVisitedReferrerBeforeDone(
    int32 download_id,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;

  // Check whether this download is for an extension install or not.
  // Allow extensions to be explicitly saved.
  DownloadStateInfo state = download->GetStateInfo();

  if (state.force_file_name.empty()) {
    FilePath generated_name;
    download_util::GenerateFileNameFromRequest(*download,
                                               &generated_name);

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
      if (!IsExtensionDownload(download) &&
          !ShouldOpenFileBasedOnExtension(generated_name))
        state.prompt_user_for_save_location = true;
    }
    if (download_prefs_->IsDownloadPathManaged()) {
      state.prompt_user_for_save_location = false;
    }

    // Determine the proper path for a download, by either one of the following:
    // 1) using the default download directory.
    // 2) prompting the user.
    if (state.prompt_user_for_save_location &&
        !download_manager_->LastDownloadPath().empty()) {
      state.suggested_path = download_manager_->LastDownloadPath();
    } else {
      state.suggested_path = download_prefs_->download_path();
    }
    state.suggested_path = state.suggested_path.Append(generated_name);
  } else {
    state.suggested_path = state.force_file_name;
  }

  // If the download hasn't already been marked dangerous (could be
  // DANGEROUS_URL), check if it is a dangerous file.
  if (state.danger == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    if (!state.prompt_user_for_save_location &&
        state.force_file_name.empty() &&
        IsDangerousFile(*download, state, visited_referrer_before)) {
      state.danger = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    }

#if defined(ENABLE_SAFE_BROWSING)
    DownloadProtectionService* service = GetDownloadProtectionService();
    // Return false if this type of files is handled by the enhanced
    // SafeBrowsing download protection.
    if (service && service->enabled()) {
      DownloadProtectionService::DownloadInfo info =
          DownloadProtectionService::DownloadInfo::FromDownloadItem(*download);
      info.target_file = state.suggested_path;
      // TODO(noelutz): if the user changes the extension name in the UI to
      // something like .exe SafeBrowsing will currently *not* check if the
      // download is malicious.
      if (service->IsSupportedDownload(info))
        state.danger = content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    }
#endif
  }

  // We need to move over to the download thread because we don't want to stat
  // the suggested path on the UI thread.
  // We can only access preferences on the UI thread, so check the download path
  // now and pass the value to the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ChromeDownloadManagerDelegate::CheckIfSuggestedPathExists,
                 this, download->GetId(), state,
                 download_prefs_->download_path()));
}

void ChromeDownloadManagerDelegate::CheckIfSuggestedPathExists(
    int32 download_id,
    DownloadStateInfo state,
    const FilePath& default_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure the default download directory exists.
  // TODO(phajdan.jr): only create the directory when we're sure the user
  // is going to save there and not to another directory of his choice.
  file_util::CreateDirectory(default_path);

  // Check writability of the suggested path. If we can't write to it, default
  // to the user's "My Documents" directory. We'll prompt them in this case.
  FilePath dir = state.suggested_path.DirName();
  FilePath filename = state.suggested_path.BaseName();
  if (!file_util::PathIsWritable(dir)) {
    VLOG(1) << "Unable to write to directory \"" << dir.value() << "\"";
    state.prompt_user_for_save_location = true;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &dir);
    state.suggested_path = dir.Append(filename);
  }

  // If the download is possibly dangerous, we'll use a temporary name for it.
  if (state.danger != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    state.target_name = FilePath(state.suggested_path).BaseName();
    // Create a temporary file to hold the file until the user approves its
    // download.
    FilePath::StringType file_name;
    FilePath path;
#if defined(OS_WIN)
    string16 unconfirmed_prefix =
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#else
    std::string unconfirmed_prefix =
        l10n_util::GetStringUTF8(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#endif

    while (path.empty()) {
      base::SStringPrintf(
          &file_name,
          unconfirmed_prefix.append(
              FILE_PATH_LITERAL(" %d.crdownload")).c_str(),
          base::RandInt(0, 100000));
      path = dir.Append(file_name);
      if (file_util::PathExists(path))
        path = FilePath();
    }
    state.suggested_path = path;
  } else {
    // Do not add the path uniquifier if we are saving to a specific path as in
    // the drag-out case.
    if (state.force_file_name.empty()) {
      state.path_uniquifier = download_util::GetUniquePathNumberWithCrDownload(
          state.suggested_path);
    }
    // We know the final path, build it if necessary.
    if (state.path_uniquifier > 0) {
      state.suggested_path = state.suggested_path.InsertBeforeExtensionASCII(
          StringPrintf(" (%d)", state.path_uniquifier));
      // Setting path_uniquifier to 0 to make sure we don't try to unique it
      // later on.
      state.path_uniquifier = 0;
    } else if (state.path_uniquifier == -1) {
      // We failed to find a unique path.  We have to prompt the user.
      VLOG(1) << "Unable to find a unique path for suggested path \""
              << state.suggested_path.value() << "\"";
      state.prompt_user_for_save_location = true;
    }
  }

  // Create an empty file at the suggested path so that we don't allocate the
  // same "non-existant" path to multiple downloads.
  // See: http://code.google.com/p/chromium/issues/detail?id=3662
  if (!state.prompt_user_for_save_location &&
      state.force_file_name.empty()) {
    if (state.danger != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
      file_util::WriteFile(state.suggested_path, "", 0);
    else
      file_util::WriteFile(download_util::GetCrDownloadPath(
          state.suggested_path), "", 0);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeDownloadManagerDelegate::OnPathExistenceAvailable,
                 this, download_id, state));
}

void ChromeDownloadManagerDelegate::OnPathExistenceAvailable(
    int32 download_id,
    const DownloadStateInfo& new_state) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;
  download->SetFileCheckResults(new_state);
  download_manager_->RestartDownload(download_id);
}

// TODO(phajdan.jr): This is apparently not being exercised in tests.
bool ChromeDownloadManagerDelegate::IsDangerousFile(
    const DownloadItem& download,
    const DownloadStateInfo& state,
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Anything loaded directly from the address bar is OK.
  if (state.transition_type & content::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    return false;

  // Extensions that are not from the gallery are considered dangerous.
  // When off-store install is disabled we skip this, since in this case, we
  // will not offer to install the extension.
  if (extensions::switch_utils::IsOffStoreInstallEnabled() &&
      IsExtensionDownload(&download) &&
      !WebstoreInstaller::GetAssociatedApproval(download)) {
    return true;
  }

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (ShouldOpenFileBasedOnExtension(state.suggested_path) &&
      state.has_user_gesture)
    return false;

  // "Allow on user gesture" is OK when we have a user gesture and the hosting
  // page has been visited before today.
  download_util::DownloadDangerLevel danger_level =
      download_util::GetFileDangerLevel(state.suggested_path.BaseName());
  if (danger_level == download_util::AllowOnUserGesture)
    return !state.has_user_gesture || !visited_referrer_before;

  return danger_level == download_util::Dangerous;
}

void ChromeDownloadManagerDelegate::OnItemAddedToPersistentStore(
    int32 download_id, int64 db_handle) {
  // It's not immediately obvious, but HistoryBackend::CreateDownload() can
  // call this function with an invalid |db_handle|. For instance, this can
  // happen when the history database is offline. We cannot have multiple
  // DownloadItems with the same invalid db_handle, so we need to assign a
  // unique |db_handle| here.
  if (db_handle == DownloadItem::kUninitializedHandle)
    db_handle = download_history_->GetNextFakeDbHandle();
  download_manager_->OnItemAddedToPersistentStore(download_id, db_handle);
}
