// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_target_determiner.h"

#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/download_handler.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

using content::BrowserThread;
using content::DownloadItem;

namespace {

// Condenses the results from HistoryService::GetVisibleVisitCountToHost() to a
// single bool. A host is considered visited before if prior visible visits were
// found in history and the first such visit was earlier than the most recent
// midnight.
void VisitCountsToVisitedBefore(
    const base::Callback<void(bool)>& callback,
    HistoryService::Handle unused_handle,
    bool found_visits,
    int count,
    base::Time first_visit) {
  callback.Run(
      found_visits && count > 0 &&
      (first_visit.LocalMidnight() < base::Time::Now().LocalMidnight()));
}

} // namespace

DownloadTargetDeterminerDelegate::~DownloadTargetDeterminerDelegate() {
}

DownloadTargetDeterminer::DownloadTargetDeterminer(
    DownloadItem* download,
    DownloadPrefs* download_prefs,
    const base::FilePath& last_selected_directory,
    DownloadTargetDeterminerDelegate* delegate,
    const content::DownloadTargetCallback& callback)
    : next_state_(STATE_GENERATE_TARGET_PATH),
      should_prompt_(false),
      create_directory_(false),
      conflict_action_(download->GetForcedFilePath().empty() ?
                       DownloadPathReservationTracker::UNIQUIFY :
                       DownloadPathReservationTracker::OVERWRITE),
      danger_type_(download->GetDangerType()),
      download_(download),
      download_prefs_(download_prefs),
      delegate_(delegate),
      last_selected_directory_(last_selected_directory),
      completion_callback_(callback),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download_);
  DCHECK(delegate);
  download_->AddObserver(this);

  DoLoop();
}

DownloadTargetDeterminer::~DownloadTargetDeterminer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(download_);
  DCHECK(completion_callback_.is_null());
  download_->RemoveObserver(this);
}

void DownloadTargetDeterminer::DoLoop() {
  Result result = CONTINUE;
  do {
    State current_state = next_state_;
    next_state_ = STATE_NONE;

    switch (current_state) {
      case STATE_GENERATE_TARGET_PATH:
        result = DoGenerateTargetPath();
        break;
      case STATE_NOTIFY_EXTENSIONS:
        result = DoNotifyExtensions();
        break;
      case STATE_RESERVE_VIRTUAL_PATH:
        result = DoReserveVirtualPath();
        break;
      case STATE_PROMPT_USER_FOR_DOWNLOAD_PATH:
        result = DoPromptUserForDownloadPath();
        break;
      case STATE_DETERMINE_LOCAL_PATH:
        result = DoDetermineLocalPath();
        break;
      case STATE_CHECK_DOWNLOAD_URL:
        result = DoCheckDownloadUrl();
        break;
      case STATE_DETERMINE_INTERMEDIATE_PATH:
        result = DoDetermineIntermediatePath();
        break;
      case STATE_CHECK_VISITED_REFERRER_BEFORE:
        result = DoCheckVisitedReferrerBefore();
        break;
      case STATE_NONE:
        NOTREACHED();
        return;
    }
  } while (result == CONTINUE);
  // Note that if a callback completes synchronously, the handler will still
  // return QUIT_DOLOOP. In this case, an inner DoLoop() may complete the target
  // determination and delete |this|.

  if (result == COMPLETE)
    ScheduleCallbackAndDeleteSelf();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoGenerateTargetPath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(virtual_path_.empty());
  DCHECK(local_path_.empty());
  bool is_forced_path = !download_->GetForcedFilePath().empty();

  next_state_ = STATE_NOTIFY_EXTENSIONS;

  // If we don't have a forced path, we should construct a path for the
  // download. Forced paths are only specified for programmatic downloads
  // (WebStore, Drag&Drop). Treat the path as a virtual path. We will eventually
  // determine whether this is a local path and if not, figure out a local path.
  if (!is_forced_path) {
   std::string default_filename(
        l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
    base::FilePath generated_filename = net::GenerateFileName(
        download_->GetURL(),
        download_->GetContentDisposition(),
        GetProfile()->GetPrefs()->GetString(prefs::kDefaultCharset),
        download_->GetSuggestedFilename(),
        download_->GetMimeType(),
        default_filename);
    should_prompt_ = ShouldPromptForDownload(generated_filename);
    base::FilePath target_directory;
    if (should_prompt_ && !last_selected_directory_.empty()) {
      DCHECK(!download_prefs_->IsDownloadPathManaged());
      // If the user is going to be prompted and the user has been prompted
      // before, then always prefer the last directory that the user selected.
      target_directory = last_selected_directory_;
    } else {
      target_directory = download_prefs_->DownloadPath();
    }
    virtual_path_ = target_directory.Append(generated_filename);
  } else {
    DCHECK(!should_prompt_);
    virtual_path_ = download_->GetForcedFilePath();
  }
  DCHECK(virtual_path_.IsAbsolute());
  DVLOG(20) << "Generated virtual path: " << virtual_path_.AsUTF8Unsafe();

  // If the download is DOA, don't bother going any further. This would be the
  // case for a download that failed to initialize (e.g. the initial temporary
  // file couldn't be created because both the downloads directory and the
  // temporary directory are unwriteable).
  //
  // A virtual path is determined for DOA downloads for display purposes. This
  // is why this check is performed here instead of at the start.
  if (!download_->IsInProgress())
    return COMPLETE;
  return CONTINUE;
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoNotifyExtensions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_RESERVE_VIRTUAL_PATH;

  // If the target path is forced or if we don't have an extensions event
  // router, then proceed with the original path.
  if (!download_->GetForcedFilePath().empty())
    return CONTINUE;

  delegate_->NotifyExtensions(download_, virtual_path_,
      base::Bind(&DownloadTargetDeterminer::NotifyExtensionsDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::NotifyExtensionsDone(
    const base::FilePath& suggested_path,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "Extension suggested path: " << suggested_path.AsUTF8Unsafe();

  if (!suggested_path.empty()) {
    // If an extension overrides the filename, then the target directory will be
    // forced to download_prefs_->DownloadPath() since extensions cannot place
    // downloaded files anywhere except there. This prevents subdirectories from
    // accumulating: if an extension is allowed to say that a file should go in
    // last_download_path/music/foo.mp3, then last_download_path will accumulate
    // the subdirectory /music/ so that the next download may end up in
    // Downloads/music/music/music/bar.mp3.
    base::FilePath new_path(download_prefs_->DownloadPath().Append(
        suggested_path).NormalizePathSeparators());
    // Do not pass a mime type to GenerateSafeFileName so that it does not force
    // the filename to have an extension if the (Chrome) extension does not
    // suggest it.
    net::GenerateSafeFileName(std::string(), false, &new_path);
    virtual_path_ = new_path;
    create_directory_ = true;
    conflict_action_ = conflict_action;
  }

  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoReserveVirtualPath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_PROMPT_USER_FOR_DOWNLOAD_PATH;

  delegate_->ReserveVirtualPath(
      download_, virtual_path_, create_directory_, conflict_action_,
      base::Bind(&DownloadTargetDeterminer::ReserveVirtualPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::ReserveVirtualPathDone(
    const base::FilePath& path, bool verified) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "Reserved path: " << path.AsUTF8Unsafe()
            << " Verified:" << verified;
  should_prompt_ = (should_prompt_ || !verified);
  virtual_path_ = path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoPromptUserForDownloadPath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_DETERMINE_LOCAL_PATH;

  if (should_prompt_) {
    delegate_->PromptUserForDownloadPath(
        download_,
        virtual_path_,
        base::Bind(&DownloadTargetDeterminer::PromptUserForDownloadPathDone,
                   weak_ptr_factory_.GetWeakPtr()));
    return QUIT_DOLOOP;
  }
  return CONTINUE;
}

void DownloadTargetDeterminer::PromptUserForDownloadPathDone(
    const base::FilePath& virtual_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "User selected path:" << virtual_path.AsUTF8Unsafe();
  if (virtual_path.empty()) {
    CancelOnFailureAndDeleteSelf();
    return;
  }
  virtual_path_ = virtual_path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineLocalPath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());
  DCHECK(local_path_.empty());

  next_state_ = STATE_CHECK_DOWNLOAD_URL;

  delegate_->DetermineLocalPath(
      download_,
      virtual_path_,
      base::Bind(&DownloadTargetDeterminer::DetermineLocalPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::DetermineLocalPathDone(
    const base::FilePath& local_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "Local path: " << local_path.AsUTF8Unsafe();
  if (local_path.empty()) {
    // Path subsitution failed.
    CancelOnFailureAndDeleteSelf();
    return;
  }
  local_path_ = local_path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckDownloadUrl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());
  next_state_ = STATE_CHECK_VISITED_REFERRER_BEFORE;
  delegate_->CheckDownloadUrl(
      download_,
      virtual_path_,
      base::Bind(&DownloadTargetDeterminer::CheckDownloadUrlDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::CheckDownloadUrlDone(
    content::DownloadDangerType danger_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "URL Check Result:" << danger_type;
  danger_type_ = danger_type;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckVisitedReferrerBefore() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  next_state_ = STATE_DETERMINE_INTERMEDIATE_PATH;

  // Checking if there are prior visits to the referrer is only necessary if the
  // danger level of the download depends on the file type. This excludes cases
  // where the download has already been deemed dangerous, or where the user is
  // going to be prompted or where this is a programmatic download.
  if (danger_type_ != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
      should_prompt_ ||
      !download_->GetForcedFilePath().empty()) {
    return CONTINUE;
  }

  // Assume that:
  // IsDangerousFile(VISITED_REFERRER) => IsDangerousFile(NO_VISITS_...)
  // I.e. having visited a referrer only lowers a file's danger level.
  if (IsDangerousFile(NO_VISITS_TO_REFERRER)) {
    // Only need to ping the history DB if the download would be considered safe
    // if there are prior visits and is considered dangerous otherwise.
    if (!IsDangerousFile(VISITED_REFERRER)) {
      // HistoryServiceFactory redirects incognito profiles to on-record
      // profiles.  There's no history for on-record profiles in unit_tests.
      HistoryService* history_service = HistoryServiceFactory::GetForProfile(
          GetProfile(), Profile::EXPLICIT_ACCESS);

      if (history_service && download_->GetReferrerUrl().is_valid()) {
        history_service->GetVisibleVisitCountToHost(
            download_->GetReferrerUrl(), &history_consumer_,
            base::Bind(&VisitCountsToVisitedBefore, base::Bind(
                &DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone,
                weak_ptr_factory_.GetWeakPtr())));
        return QUIT_DOLOOP;
      }
    }

    // If the danger level doesn't depend on having visited the refererrer URL
    // or if original profile doesn't have a HistoryService or the referrer url
    // is invalid, then assume the referrer has not been visited before.
    danger_type_ = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  }
  return CONTINUE;
}

void DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone(
    bool visited_referrer_before) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsDangerousFile(
          visited_referrer_before ? VISITED_REFERRER : NO_VISITS_TO_REFERRER))
    danger_type_ = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIntermediatePath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(intermediate_path_.empty());

  next_state_ = STATE_NONE;

  // Note that the intermediate filename is always uniquified (i.e. if a file by
  // the same name exists, it is never overwritten). Therefore the code below
  // does not attempt to find a name that doesn't conflict with an existing
  // file.

  // If the actual target of the download is a virtual path, then the local path
  // is considered to point to a temporary path. A separate intermediate path is
  // unnecessary since the local path already serves that purpose.
  if (virtual_path_.BaseName() != local_path_.BaseName()) {
    intermediate_path_ = local_path_;
    return COMPLETE;
  }

  // If the download has a forced path and is safe, then just use the
  // target path. In practice the temporary download file that was created prior
  // to download filename determination is already named
  // download_->GetForcedFilePath().
  if (danger_type_ == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS &&
      !download_->GetForcedFilePath().empty()) {
    DCHECK_EQ(download_->GetForcedFilePath().value(), local_path_.value());
    intermediate_path_ = local_path_;
    return COMPLETE;
  }

  // Other safe downloads get a .crdownload suffix for their intermediate name.
  if (danger_type_ == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    intermediate_path_ = download_util::GetCrDownloadPath(local_path_);
    return COMPLETE;
  }

  // Dangerous downloads receive a random intermediate name that looks like:
  // 'Unconfirmed <random>.crdownload'.
  const base::FilePath::CharType kUnconfirmedFormatSuffix[] =
      FILE_PATH_LITERAL(" %d.crdownload");
  // Range of the <random> uniquifier.
  const int kUnconfirmedUniquifierRange = 1000000;
#if defined(OS_WIN)
  string16 unconfirmed_format =
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#else
  std::string unconfirmed_format =
      l10n_util::GetStringUTF8(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#endif
  unconfirmed_format.append(kUnconfirmedFormatSuffix);

  base::FilePath::StringType file_name = base::StringPrintf(
      unconfirmed_format.c_str(),
      base::RandInt(0, kUnconfirmedUniquifierRange));
  intermediate_path_ = local_path_.DirName().Append(file_name);
  return COMPLETE;
}

void DownloadTargetDeterminer::ScheduleCallbackAndDeleteSelf() {
  DCHECK(download_);
  DVLOG(20) << "Scheduling callback. Virtual:" << virtual_path_.AsUTF8Unsafe()
            << " Local:" << local_path_.AsUTF8Unsafe()
            << " Intermediate:" << intermediate_path_.AsUTF8Unsafe()
            << " Should prompt:" << should_prompt_
            << " Danger type:" << danger_type_;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(completion_callback_,
                 local_path_,
                 (should_prompt_ ? DownloadItem::TARGET_DISPOSITION_PROMPT :
                                   DownloadItem::TARGET_DISPOSITION_OVERWRITE),
                 danger_type_,
                 intermediate_path_));
  completion_callback_.Reset();
  delete this;
}

void DownloadTargetDeterminer::CancelOnFailureAndDeleteSelf() {
  // Path substitution failed.
  virtual_path_.clear();
  local_path_.clear();
  intermediate_path_.clear();
  ScheduleCallbackAndDeleteSelf();
}

Profile* DownloadTargetDeterminer::GetProfile() {
  DCHECK(download_->GetBrowserContext());
  return Profile::FromBrowserContext(download_->GetBrowserContext());
}

bool DownloadTargetDeterminer::ShouldPromptForDownload(
    const base::FilePath& filename) {
  // If the download path is forced, don't prompt.
  if (!download_->GetForcedFilePath().empty()) {
    // 'Save As' downloads shouldn't have a forced path.
    DCHECK_NE(DownloadItem::TARGET_DISPOSITION_PROMPT,
              download_->GetTargetDisposition());
    return false;
  }

  // Don't ask where to save if the download path is managed. Even if the user
  // wanted to be prompted for "all" downloads, or if this was a 'Save As'
  // download.
  if (download_prefs_->IsDownloadPathManaged())
    return false;

  // Prompt if this is a 'Save As' download.
  if (download_->GetTargetDisposition() ==
      DownloadItem::TARGET_DISPOSITION_PROMPT)
    return true;

  // Check if the user has the "Always prompt for download location" preference
  // set. If so we prompt for most downloads except for the following scenarios:
  // 1) Extension installation. Note that we only care here about the case where
  //    an extension is installed, not when one is downloaded with "save as...".
  // 2) Filetypes marked "always open." If the user just wants this file opened,
  //    don't bother asking where to keep it.
  if (download_prefs_->PromptForDownload() &&
      !download_crx_util::IsExtensionDownload(*download_) &&
      !extensions::Extension::IsExtension(filename) &&
      !download_prefs_->IsAutoOpenEnabledBasedOnExtension(filename))
    return true;

  // Otherwise, don't prompt. Note that the user might still be prompted if
  // there are unresolved conflicts during path reservation (e.g. due to the
  // target path being unwriteable or because there are too many conflicting
  // files), or if an extension signals that the user be prompted on a filename
  // conflict.
  return false;
}

bool DownloadTargetDeterminer::IsDangerousFile(PriorVisitsToReferrer visits) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const bool is_extension_download =
      download_crx_util::IsExtensionDownload(*download_);

  // User-initiated extension downloads from pref-whitelisted sources are not
  // considered dangerous.
  if (download_->HasUserGesture() &&
      is_extension_download &&
      download_crx_util::OffStoreInstallAllowedByPrefs(
          GetProfile(), *download_)) {
    return false;
  }

  // Extensions that are not from the gallery are considered dangerous.
  // When off-store install is disabled we skip this, since in this case, we
  // will not offer to install the extension.
  if (extensions::FeatureSwitch::easy_off_store_install()->IsEnabled() &&
      is_extension_download &&
      !extensions::WebstoreInstaller::GetAssociatedApproval(*download_)) {
    return true;
  }

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (download_prefs_->IsAutoOpenEnabledBasedOnExtension(virtual_path_) &&
      download_->HasUserGesture())
    return false;

  switch (download_util::GetFileDangerLevel(virtual_path_.BaseName())) {
    case download_util::NotDangerous:
      return false;

    case download_util::AllowOnUserGesture:
      // "Allow on user gesture" is OK when we have a user gesture and the
      // hosting page has been visited before today.
      if (download_->GetTransitionType() &
          content::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
        return false;
      }
      return !download_->HasUserGesture() || visits == NO_VISITS_TO_REFERRER;

    case download_util::Dangerous:
      return true;
  }
  NOTREACHED();
  return false;
}

void DownloadTargetDeterminer::OnDownloadDestroyed(
    DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(download_, download);
  CancelOnFailureAndDeleteSelf();
}

// static
void DownloadTargetDeterminer::Start(
    content::DownloadItem* download,
    DownloadPrefs* download_prefs,
    const base::FilePath& last_selected_directory,
    DownloadTargetDeterminerDelegate* delegate,
    const content::DownloadTargetCallback& callback) {
  // DownloadTargetDeterminer owns itself and will self destruct when the job is
  // complete or the download item is destroyed. The callback is always invoked
  // asynchronously.
  new DownloadTargetDeterminer(download, download_prefs,
                               last_selected_directory, delegate, callback);
}
