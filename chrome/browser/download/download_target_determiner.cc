// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_target_determiner.h"

#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/mime_util/mime_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "extensions/common/constants.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/webstore_installer.h"
#include "extensions/common/feature_switch.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_prefs.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using content::BrowserThread;
using content::DownloadItem;

namespace {

const base::FilePath::CharType kCrdownloadSuffix[] =
    FILE_PATH_LITERAL(".crdownload");

// Condenses the results from HistoryService::GetVisibleVisitCountToHost() to a
// single bool. A host is considered visited before if prior visible visits were
// found in history and the first such visit was earlier than the most recent
// midnight.
void VisitCountsToVisitedBefore(
    const base::Callback<void(bool)>& callback,
    bool found_visits,
    int count,
    base::Time first_visit) {
  callback.Run(
      found_visits && count > 0 &&
      (first_visit.LocalMidnight() < base::Time::Now().LocalMidnight()));
}

#if defined(OS_WIN)
// Keeps track of whether Adobe Reader is up to date.
bool g_is_adobe_reader_up_to_date_ = false;
#endif

}  // namespace

DownloadTargetInfo::DownloadTargetInfo()
    : target_disposition(DownloadItem::TARGET_DISPOSITION_OVERWRITE),
      danger_type(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      danger_level(download_util::NOT_DANGEROUS),
      is_filetype_handled_safely(false) {}

DownloadTargetInfo::~DownloadTargetInfo() {}

DownloadTargetDeterminerDelegate::~DownloadTargetDeterminerDelegate() {
}

DownloadTargetDeterminer::DownloadTargetDeterminer(
    DownloadItem* download,
    const base::FilePath& initial_virtual_path,
    DownloadPrefs* download_prefs,
    DownloadTargetDeterminerDelegate* delegate,
    const CompletionCallback& callback)
    : next_state_(STATE_GENERATE_TARGET_PATH),
      should_prompt_(false),
      should_notify_extensions_(false),
      create_target_directory_(false),
      conflict_action_(DownloadPathReservationTracker::OVERWRITE),
      danger_type_(download->GetDangerType()),
      danger_level_(download_util::NOT_DANGEROUS),
      virtual_path_(initial_virtual_path),
      is_filetype_handled_safely_(false),
      download_(download),
      is_resumption_(download_->GetLastReason() !=
                         content::DOWNLOAD_INTERRUPT_REASON_NONE &&
                     !initial_virtual_path.empty()),
      download_prefs_(download_prefs),
      delegate_(delegate),
      completion_callback_(callback),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_);
  DCHECK(delegate);
  download_->AddObserver(this);

  DoLoop();
}

DownloadTargetDeterminer::~DownloadTargetDeterminer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
      case STATE_DETERMINE_MIME_TYPE:
        result = DoDetermineMimeType();
        break;
      case STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER:
        result = DoDetermineIfHandledSafely();
        break;
      case STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE:
        result = DoDetermineIfAdobeReaderUpToDate();
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(local_path_.empty());
  DCHECK(!should_prompt_);
  DCHECK(!should_notify_extensions_);
  DCHECK_EQ(DownloadPathReservationTracker::OVERWRITE, conflict_action_);
  bool is_forced_path = !download_->GetForcedFilePath().empty();

  next_state_ = STATE_NOTIFY_EXTENSIONS;

  if (!virtual_path_.empty() && HasPromptedForPath() && !is_forced_path) {
    // The download is being resumed and the user has already been prompted for
    // a path. Assume that it's okay to overwrite the file if there's a conflict
    // and reuse the selection.
    should_prompt_ = ShouldPromptForDownload(virtual_path_);
  } else if (!is_forced_path) {
    // If we don't have a forced path, we should construct a path for the
    // download. Forced paths are only specified for programmatic downloads
    // (WebStore, Drag&Drop). Treat the path as a virtual path. We will
    // eventually determine whether this is a local path and if not, figure out
    // a local path.

    std::string suggested_filename = download_->GetSuggestedFilename();
    if (suggested_filename.empty() &&
        download_->GetMimeType() == "application/x-x509-user-cert") {
      suggested_filename = "user.crt";
    }

    std::string default_filename(
        l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
    base::FilePath generated_filename = net::GenerateFileName(
        download_->GetURL(),
        download_->GetContentDisposition(),
        GetProfile()->GetPrefs()->GetString(prefs::kDefaultCharset),
        suggested_filename,
        download_->GetMimeType(),
        default_filename);
    should_prompt_ = ShouldPromptForDownload(generated_filename);
    base::FilePath target_directory;
    if (should_prompt_) {
      DCHECK(!download_prefs_->IsDownloadPathManaged());
      // If the user is going to be prompted and the user has been prompted
      // before, then always prefer the last directory that the user selected.
      target_directory = download_prefs_->SaveFilePath();
    } else {
      target_directory = download_prefs_->DownloadPath();
    }
    virtual_path_ = target_directory.Append(generated_filename);
#if defined(OS_ANDROID)
    conflict_action_ = DownloadPathReservationTracker::PROMPT;
#else
    conflict_action_ = DownloadPathReservationTracker::UNIQUIFY;
#endif
    should_notify_extensions_ = true;
  } else {
    virtual_path_ = download_->GetForcedFilePath();
    // If this is a resumed download which was previously interrupted due to an
    // issue with the forced path, the user is still not prompted. If the path
    // supplied to a programmatic download is invalid, then the caller needs to
    // intervene.
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
  if (download_->GetState() != DownloadItem::IN_PROGRESS)
    return COMPLETE;
  return CONTINUE;
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoNotifyExtensions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_RESERVE_VIRTUAL_PATH;

  if (!should_notify_extensions_)
    return CONTINUE;

  delegate_->NotifyExtensions(download_, virtual_path_,
      base::Bind(&DownloadTargetDeterminer::NotifyExtensionsDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::NotifyExtensionsDone(
    const base::FilePath& suggested_path,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Extension suggested path: " << suggested_path.AsUTF8Unsafe();

  // Extensions should not call back here more than once.
  DCHECK_EQ(STATE_RESERVE_VIRTUAL_PATH, next_state_);

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
    create_target_directory_ = true;
  }
  // An extension may set conflictAction without setting filename.
  if (conflict_action != DownloadPathReservationTracker::UNIQUIFY)
    conflict_action_ = conflict_action;

  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoReserveVirtualPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_PROMPT_USER_FOR_DOWNLOAD_PATH;

  delegate_->ReserveVirtualPath(
      download_, virtual_path_, create_target_directory_, conflict_action_,
      base::Bind(&DownloadTargetDeterminer::ReserveVirtualPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::ReserveVirtualPathDone(
    const base::FilePath& path, bool verified) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Reserved path: " << path.AsUTF8Unsafe()
            << " Verified:" << verified;
  DCHECK_EQ(STATE_PROMPT_USER_FOR_DOWNLOAD_PATH, next_state_);

  should_prompt_ = (should_prompt_ || !verified);
  virtual_path_ = path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoPromptUserForDownloadPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "User selected path:" << virtual_path.AsUTF8Unsafe();
  if (virtual_path.empty()) {
    CancelOnFailureAndDeleteSelf();
    return;
  }
  DCHECK_EQ(STATE_DETERMINE_LOCAL_PATH, next_state_);

  virtual_path_ = virtual_path;
  download_prefs_->SetSaveFilePath(virtual_path_.DirName());
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineLocalPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(local_path_.empty());

  next_state_ = STATE_DETERMINE_MIME_TYPE;

  delegate_->DetermineLocalPath(
      download_,
      virtual_path_,
      base::Bind(&DownloadTargetDeterminer::DetermineLocalPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::DetermineLocalPathDone(
    const base::FilePath& local_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Local path: " << local_path.AsUTF8Unsafe();
  if (local_path.empty()) {
    // Path subsitution failed.
    CancelOnFailureAndDeleteSelf();
    return;
  }
  DCHECK_EQ(STATE_DETERMINE_MIME_TYPE, next_state_);

  local_path_ = local_path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineMimeType() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(mime_type_.empty());

  next_state_ = STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER;

  if (virtual_path_ == local_path_) {
    delegate_->GetFileMimeType(
        local_path_,
        base::Bind(&DownloadTargetDeterminer::DetermineMimeTypeDone,
                   weak_ptr_factory_.GetWeakPtr()));
    return QUIT_DOLOOP;
  }
  return CONTINUE;
}

void DownloadTargetDeterminer::DetermineMimeTypeDone(
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "MIME type: " << mime_type;
  DCHECK_EQ(STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER, next_state_);

  mime_type_ = mime_type;
  DoLoop();
}

#if defined(ENABLE_PLUGINS)
// The code below is used by DoDetermineIfHandledSafely to determine if the
// file type is handled by a sandboxed plugin.
namespace {

void InvokeClosureAfterGetPluginCallback(
    const base::Closure& closure,
    const std::vector<content::WebPluginInfo>& unused) {
  closure.Run();
}

enum ActionOnStalePluginList {
  RETRY_IF_STALE_PLUGIN_LIST,
  IGNORE_IF_STALE_PLUGIN_LIST
};

void IsHandledBySafePlugin(content::ResourceContext* resource_context,
                           const GURL& url,
                           const std::string& mime_type,
                           ActionOnStalePluginList stale_plugin_action,
                           const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!mime_type.empty());
  using content::WebPluginInfo;

  std::string actual_mime_type;
  bool is_stale = false;
  WebPluginInfo plugin_info;

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  bool plugin_found = plugin_service->GetPluginInfo(-1, -1, resource_context,
                                                    url, GURL(), mime_type,
                                                    false, &is_stale,
                                                    &plugin_info,
                                                    &actual_mime_type);
  if (is_stale && stale_plugin_action == RETRY_IF_STALE_PLUGIN_LIST) {
    // The GetPlugins call causes the plugin list to be refreshed. Once that's
    // done we can retry the GetPluginInfo call. We break out of this cycle
    // after a single retry in order to avoid retrying indefinitely.
    plugin_service->GetPlugins(
        base::Bind(&InvokeClosureAfterGetPluginCallback,
                   base::Bind(&IsHandledBySafePlugin,
                              resource_context,
                              url,
                              mime_type,
                              IGNORE_IF_STALE_PLUGIN_LIST,
                              callback)));
    return;
  }
  // In practice, we assume that retrying once is enough.
  DCHECK(!is_stale);
  bool is_handled_safely =
      plugin_found &&
      (plugin_info.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS ||
       plugin_info.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS ||
       plugin_info.type == WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, is_handled_safely));
}

}  // namespace
#endif  // defined(ENABLE_PLUGINS)

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIfHandledSafely() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(!is_filetype_handled_safely_);

  next_state_ = STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE;

  if (mime_type_.empty())
    return CONTINUE;

  if (mime_util::IsSupportedMimeType(mime_type_)) {
    is_filetype_handled_safely_ = true;
    return CONTINUE;
  }

#if defined(ENABLE_PLUGINS)
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &IsHandledBySafePlugin,
          GetProfile()->GetResourceContext(),
          net::FilePathToFileURL(local_path_),
          mime_type_,
          RETRY_IF_STALE_PLUGIN_LIST,
          base::Bind(&DownloadTargetDeterminer::DetermineIfHandledSafelyDone,
                     weak_ptr_factory_.GetWeakPtr())));
  return QUIT_DOLOOP;
#else
  return CONTINUE;
#endif
}

#if defined(ENABLE_PLUGINS)
void DownloadTargetDeterminer::DetermineIfHandledSafelyDone(
    bool is_handled_safely) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Is file type handled safely: " << is_filetype_handled_safely_;
  DCHECK_EQ(STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE, next_state_);
  is_filetype_handled_safely_ = is_handled_safely;
  DoLoop();
}
#endif

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIfAdobeReaderUpToDate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  next_state_ = STATE_CHECK_DOWNLOAD_URL;

#if defined(OS_WIN)
  if (!local_path_.MatchesExtension(FILE_PATH_LITERAL(".pdf")))
    return CONTINUE;
  if (!IsAdobeReaderDefaultPDFViewer()) {
    g_is_adobe_reader_up_to_date_ = false;
    return CONTINUE;
  }

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&::IsAdobeReaderUpToDate),
      base::Bind(&DownloadTargetDeterminer::DetermineIfAdobeReaderUpToDateDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
#else
  return CONTINUE;
#endif
}

#if defined(OS_WIN)
void DownloadTargetDeterminer::DetermineIfAdobeReaderUpToDateDone(
    bool adobe_reader_up_to_date) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Is Adobe Reader Up To Date: " << adobe_reader_up_to_date;
  DCHECK_EQ(STATE_CHECK_DOWNLOAD_URL, next_state_);
  g_is_adobe_reader_up_to_date_ = adobe_reader_up_to_date;
  DoLoop();
}
#endif

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckDownloadUrl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "URL Check Result:" << danger_type;
  DCHECK_EQ(STATE_CHECK_VISITED_REFERRER_BEFORE, next_state_);
  danger_type_ = danger_type;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckVisitedReferrerBefore() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  next_state_ = STATE_DETERMINE_INTERMEDIATE_PATH;

  // Checking if there are prior visits to the referrer is only necessary if the
  // danger level of the download depends on the file type.
  if (danger_type_ != content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS &&
      danger_type_ != content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)
    return CONTINUE;

  // First determine the danger level assuming that the user doesn't have any
  // prior visits to the referrer recoreded in history. The resulting danger
  // level would be ALLOW_ON_USER_GESTURE if the level depends on the visit
  // history. In the latter case, we can query the history DB to determine if
  // there were prior reqeusts and determine the danger level again once the
  // result is available.
  danger_level_ = GetDangerLevel(NO_VISITS_TO_REFERRER);

  if (danger_level_ == download_util::NOT_DANGEROUS)
    return CONTINUE;

  if (danger_level_ == download_util::ALLOW_ON_USER_GESTURE) {
    // HistoryServiceFactory redirects incognito profiles to on-record profiles.
    // There's no history for on-record profiles in unit_tests.
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

    if (history_service && download_->GetReferrerUrl().is_valid()) {
      history_service->GetVisibleVisitCountToHost(
          download_->GetReferrerUrl(),
          base::Bind(
              &VisitCountsToVisitedBefore,
              base::Bind(
                  &DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone,
                  weak_ptr_factory_.GetWeakPtr())),
          &history_tracker_);
      return QUIT_DOLOOP;
    }
  }

  // If the danger level doesn't depend on having visited the refererrer URL or
  // if original profile doesn't have a HistoryService or the referrer url is
  // invalid, then assume the referrer has not been visited before.
  if (danger_type_ == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    danger_type_ = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  return CONTINUE;
}

void DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone(
    bool visited_referrer_before) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(STATE_DETERMINE_INTERMEDIATE_PATH, next_state_);
  danger_level_ = GetDangerLevel(
      visited_referrer_before ? VISITED_REFERRER : NO_VISITS_TO_REFERRER);
  if (danger_level_ != download_util::NOT_DANGEROUS &&
      danger_type_ == content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    danger_type_ = content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIntermediatePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(intermediate_path_.empty());
  DCHECK(!virtual_path_.MatchesExtension(kCrdownloadSuffix));
  DCHECK(!local_path_.MatchesExtension(kCrdownloadSuffix));

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
    intermediate_path_ = GetCrDownloadPath(local_path_);
    return COMPLETE;
  }

  // If this is a resumed download, then re-use the existing intermediate path
  // if one is available. A resumed download shouldn't cause a non-dangerous
  // download to be considered dangerous upon resumption. Therefore the
  // intermediate file should already be in the correct form.
  if (is_resumption_ && !download_->GetFullPath().empty() &&
      local_path_.DirName() == download_->GetFullPath().DirName()) {
    DCHECK_NE(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
              download_->GetDangerType());
    DCHECK_EQ(kCrdownloadSuffix, download_->GetFullPath().Extension());
    intermediate_path_ = download_->GetFullPath();
    return COMPLETE;
  }

  // Dangerous downloads receive a random intermediate name that looks like:
  // 'Unconfirmed <random>.crdownload'.
  const base::FilePath::CharType kUnconfirmedFormatSuffix[] =
      FILE_PATH_LITERAL(" %d.crdownload");
  // Range of the <random> uniquifier.
  const int kUnconfirmedUniquifierRange = 1000000;
#if defined(OS_WIN)
  base::string16 unconfirmed_format =
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
            << " Danger type:" << danger_type_
            << " Danger level:" << danger_level_;
  scoped_ptr<DownloadTargetInfo> target_info(new DownloadTargetInfo);

  target_info->target_path = local_path_;
  target_info->target_disposition =
      (HasPromptedForPath() || should_prompt_
           ? DownloadItem::TARGET_DISPOSITION_PROMPT
           : DownloadItem::TARGET_DISPOSITION_OVERWRITE);
  target_info->danger_type = danger_type_;
  target_info->danger_level = danger_level_;
  target_info->intermediate_path = intermediate_path_;
  target_info->mime_type = mime_type_;
  target_info->is_filetype_handled_safely = is_filetype_handled_safely_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(completion_callback_, base::Passed(&target_info)));
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

Profile* DownloadTargetDeterminer::GetProfile() const {
  DCHECK(download_->GetBrowserContext());
  return Profile::FromBrowserContext(download_->GetBrowserContext());
}

bool DownloadTargetDeterminer::ShouldPromptForDownload(
    const base::FilePath& filename) const {
  if (is_resumption_) {
    // For resumed downloads, if the target disposition or prefs require
    // prompting, the user has already been prompted. Try to respect the user's
    // selection, unless we've discovered that the target path cannot be used
    // for some reason.
    content::DownloadInterruptReason reason = download_->GetLastReason();
    return (reason == content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED ||
            reason == content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE ||
            reason == content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE);
  }

  // If the download path is forced, don't prompt.
  if (!download_->GetForcedFilePath().empty()) {
    // 'Save As' downloads shouldn't have a forced path.
    DCHECK(DownloadItem::TARGET_DISPOSITION_PROMPT !=
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
      !filename.MatchesExtension(extensions::kExtensionFileExtension) &&
      !download_prefs_->IsAutoOpenEnabledBasedOnExtension(filename))
    return true;

  // Otherwise, don't prompt. Note that the user might still be prompted if
  // there are unresolved conflicts during path reservation (e.g. due to the
  // target path being unwriteable or because there are too many conflicting
  // files), or if an extension signals that the user be prompted on a filename
  // conflict.
  return false;
}

bool DownloadTargetDeterminer::HasPromptedForPath() const {
  return (is_resumption_ && download_->GetTargetDisposition() ==
                                DownloadItem::TARGET_DISPOSITION_PROMPT);
}

download_util::DownloadDangerLevel DownloadTargetDeterminer::GetDangerLevel(
    PriorVisitsToReferrer visits) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the user has has been prompted or will be, assume that the user has
  // approved the download. A programmatic download is considered safe unless it
  // contains malware.
  if (HasPromptedForPath() || should_prompt_ ||
      !download_->GetForcedFilePath().empty())
    return download_util::NOT_DANGEROUS;

  const bool is_extension_download =
      download_crx_util::IsExtensionDownload(*download_);

  // User-initiated extension downloads from pref-whitelisted sources are not
  // considered dangerous.
  if (download_->HasUserGesture() &&
      is_extension_download &&
      download_crx_util::OffStoreInstallAllowedByPrefs(
          GetProfile(), *download_)) {
    return download_util::NOT_DANGEROUS;
  }

#if defined(ENABLE_EXTENSIONS)
  // Extensions that are not from the gallery are considered dangerous.
  // When off-store install is disabled we skip this, since in this case, we
  // will not offer to install the extension.
  if (extensions::FeatureSwitch::easy_off_store_install()->IsEnabled() &&
      is_extension_download &&
      !extensions::WebstoreInstaller::GetAssociatedApproval(*download_)) {
    return download_util::ALLOW_ON_USER_GESTURE;
  }
#endif

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (download_prefs_->IsAutoOpenEnabledBasedOnExtension(virtual_path_) &&
      download_->HasUserGesture())
    return download_util::NOT_DANGEROUS;

  download_util::DownloadDangerLevel danger_level =
      download_util::GetFileDangerLevel(virtual_path_.BaseName());

  // If the danger level is ALLOW_ON_USER_GESTURE and we have a user gesture AND
  // there was a recorded visit to the referrer prior to today, then we are
  // going to downgrade the danger_level to NOT_DANGEROUS. This prevents
  // spurious prompting for moderately dangerous files that are downloaded from
  // familiar sites.
  if (danger_level == download_util::ALLOW_ON_USER_GESTURE &&
      (download_->GetTransitionType() == ui::PAGE_TRANSITION_FROM_ADDRESS_BAR ||
       (download_->HasUserGesture() && visits == VISITED_REFERRER)))
    return download_util::NOT_DANGEROUS;
  return danger_level;
}

void DownloadTargetDeterminer::OnDownloadDestroyed(
    DownloadItem* download) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(download_, download);
  CancelOnFailureAndDeleteSelf();
}

// static
void DownloadTargetDeterminer::Start(content::DownloadItem* download,
                                     const base::FilePath& initial_virtual_path,
                                     DownloadPrefs* download_prefs,
                                     DownloadTargetDeterminerDelegate* delegate,
                                     const CompletionCallback& callback) {
  // DownloadTargetDeterminer owns itself and will self destruct when the job is
  // complete or the download item is destroyed. The callback is always invoked
  // asynchronously.
  new DownloadTargetDeterminer(download, initial_virtual_path, download_prefs,
                               delegate, callback);
}

// static
base::FilePath DownloadTargetDeterminer::GetCrDownloadPath(
    const base::FilePath& suggested_path) {
  return base::FilePath(suggested_path.value() + kCrdownloadSuffix);
}

#if defined(OS_WIN)
// static
bool DownloadTargetDeterminer::IsAdobeReaderUpToDate() {
  return g_is_adobe_reader_up_to_date_;
}
#endif
