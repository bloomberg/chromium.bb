// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Update frequency (milliseconds).
const int kUpdateTimeMs = 1000;

void DeleteDownloadedFile(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure we only delete files.
  if (!file_util::DirectoryExists(path))
    file_util::Delete(path, false);
}

const char* DebugSafetyStateString(DownloadItem::SafetyState state) {
  switch (state) {
    case DownloadItem::SAFE:
      return "SAFE";
    case DownloadItem::DANGEROUS:
      return "DANGEROUS";
    case DownloadItem::DANGEROUS_BUT_VALIDATED:
      return "DANGEROUS_BUT_VALIDATED";
    default:
      NOTREACHED() << "Unknown safety state " << state;
      return "unknown";
  };
}

const char* DebugDownloadStateString(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return "IN_PROGRESS";
    case DownloadItem::COMPLETE:
      return "COMPLETE";
    case DownloadItem::CANCELLED:
      return "CANCELLED";
    case DownloadItem::REMOVING:
      return "REMOVING";
    default:
      NOTREACHED() << "Unknown download state " << state;
      return "unknown";
  };
}

}  // namespace

// Constructor for reading from the history service.
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const DownloadCreateInfo& info)
    : id_(-1),
      full_path_(info.path),
      path_uniquifier_(0),
      url_(info.url),
      original_url_(info.original_url),
      referrer_url_(info.referrer_url),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      total_bytes_(info.total_bytes),
      received_bytes_(info.received_bytes),
      start_tick_(base::TimeTicks()),
      state_(static_cast<DownloadState>(info.state)),
      start_time_(info.start_time),
      db_handle_(info.db_handle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      target_name_(info.original_name),
      render_process_id_(-1),
      request_id_(-1),
      save_as_(false),
      is_otr_(false),
      is_extension_install_(info.is_extension_install),
      name_finalized_(false),
      is_temporary_(false),
      opened_(false) {
  if (state_ == IN_PROGRESS)
    state_ = CANCELLED;
  Init(false /* don't start progress timer */);
}

// Constructing for a regular download:
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const DownloadCreateInfo& info,
                           bool is_otr)
    : id_(info.download_id),
      full_path_(info.path),
      path_uniquifier_(info.path_uniquifier),
      url_(info.url),
      original_url_(info.original_url),
      referrer_url_(info.referrer_url),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(info.start_time),
      db_handle_(DownloadHistory::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(info.is_dangerous ? DANGEROUS : SAFE),
      auto_opened_(false),
      target_name_(info.original_name),
      render_process_id_(info.child_id),
      request_id_(info.request_id),
      save_as_(info.prompt_user_for_save_location),
      is_otr_(is_otr),
      is_extension_install_(info.is_extension_install),
      name_finalized_(false),
      is_temporary_(!info.save_info.file_path.empty()),
      opened_(false) {
  Init(true /* start progress timer */);
}

// Constructing for the "Save Page As..." feature:
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const FilePath& path,
                           const GURL& url,
                           bool is_otr)
    : id_(1),
      full_path_(path),
      path_uniquifier_(0),
      url_(url),
      original_url_(url),
      referrer_url_(GURL()),
      mime_type_(std::string()),
      original_mime_type_(std::string()),
      total_bytes_(0),
      received_bytes_(0),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(base::Time::Now()),
      db_handle_(DownloadHistory::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      render_process_id_(-1),
      request_id_(-1),
      save_as_(false),
      is_otr_(is_otr),
      is_extension_install_(false),
      name_finalized_(false),
      is_temporary_(false),
      opened_(false) {
  Init(true /* start progress timer */);
}

DownloadItem::~DownloadItem() {
  state_ = REMOVING;
  UpdateObservers();
}

void DownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DownloadItem::UpdateObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

void DownloadItem::NotifyObserversDownloadFileCompleted() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadFileCompleted(this));
}

bool DownloadItem::CanOpenDownload() {
  return !Extension::IsExtension(target_name_);
}

bool DownloadItem::ShouldOpenFileBasedOnExtension() {
  return download_manager_->ShouldOpenFileBasedOnExtension(
      GetUserVerifiedFilePath());
}

void DownloadItem::OpenFilesBasedOnExtension(bool open) {
  DownloadPrefs* prefs = download_manager_->download_prefs();
  if (open)
    prefs->EnableAutoOpenBasedOnExtension(GetUserVerifiedFilePath());
  else
    prefs->DisableAutoOpenBasedOnExtension(GetUserVerifiedFilePath());
}

void DownloadItem::OpenDownload() {
  if (state() == DownloadItem::IN_PROGRESS) {
    open_when_complete_ = !open_when_complete_;
  } else if (state() == DownloadItem::COMPLETE) {
    opened_ = true;
    FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
    if (is_extension_install()) {
      download_util::OpenChromeExtension(download_manager_->profile(),
                                         download_manager_,
                                         *this);
      return;
    }
#if defined(OS_MACOSX)
    // Mac OS X requires opening downloads on the UI thread.
    platform_util::OpenItem(full_path());
#else
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&platform_util::OpenItem, full_path()));
#endif
  }
}

void DownloadItem::ShowDownloadInShell() {
#if defined(OS_MACOSX)
  // Mac needs to run this operation on the UI thread.
  platform_util::ShowItemInFolder(full_path());
#else
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&platform_util::ShowItemInFolder,
                          full_path()));
#endif
}

void DownloadItem::DangerousDownloadValidated() {
  download_manager_->DangerousDownloadValidated(this);
}

void DownloadItem::UpdateSize(int64 bytes_so_far) {
  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;
}

void DownloadItem::StartProgressTimer() {
  update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdateTimeMs), this,
                      &DownloadItem::UpdateObservers);
}

void DownloadItem::StopProgressTimer() {
  update_timer_.Stop();
}

// Updates from the download thread may have been posted while this download
// was being cancelled in the UI thread, so we'll accept them unless we're
// complete.
void DownloadItem::Update(int64 bytes_so_far) {
  if (state_ == COMPLETE) {
    NOTREACHED();
    return;
  }
  UpdateSize(bytes_so_far);
  UpdateObservers();
}

// Triggered by a user action.
void DownloadItem::Cancel(bool update_history) {
  VLOG(20) << __FUNCTION__ << "()" << " download = " << DebugString(true);
  if (state_ != IN_PROGRESS) {
    // Small downloads might be complete before this method has a chance to run.
    return;
  }
  state_ = CANCELLED;
  UpdateObservers();
  StopProgressTimer();
  if (update_history)
    download_manager_->DownloadCancelled(id_);
}

void DownloadItem::OnAllDataSaved(int64 size) {
  state_ = COMPLETE;
  UpdateSize(size);
  StopProgressTimer();
}

void DownloadItem::Finished() {
  // Handle chrome extensions explicitly and skip the shell execute.
  if (is_extension_install()) {
    download_util::OpenChromeExtension(download_manager_->profile(),
                                       download_manager_,
                                       *this);
    auto_opened_ = true;
  } else if (open_when_complete() ||
             download_manager_->ShouldOpenFileBasedOnExtension(
                 GetUserVerifiedFilePath()) ||
             is_temporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!is_temporary())
      OpenDownload();
    auto_opened_ = true;
  }

  // Notify our observers that we are complete (the call to OnAllDataSaved()
  // set the state to complete but did not notify).
  UpdateObservers();

  // The download file is meant to be completed if both the filename is
  // finalized and the file data is downloaded. The ordering of these two
  // actions is indeterministic. Thus, if the filename is not finalized yet,
  // delay the notification.
  if (name_finalized()) {
    NotifyObserversDownloadFileCompleted();
    download_manager_->RemoveFromActiveList(id());
  }
}

void DownloadItem::Remove(bool delete_on_disk) {
  Cancel(true);
  state_ = REMOVING;
  if (delete_on_disk) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&DeleteDownloadedFile, full_path_));
  }
  download_manager_->RemoveDownload(db_handle_);
  // We have now been deleted.
}

bool DownloadItem::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining =
      base::TimeDelta::FromSeconds((total_bytes_ - received_bytes_) / speed);
  return true;
}

int64 DownloadItem::CurrentSpeed() const {
  if (is_paused_)
    return 0;
  base::TimeDelta diff = base::TimeTicks::Now() - start_tick_;
  int64 diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : received_bytes_ * 1000 / diff_ms;
}

int DownloadItem::PercentComplete() const {
  int percent = -1;
  if (total_bytes_ > 0)
    percent = static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
  return percent;
}

void DownloadItem::Rename(const FilePath& full_path) {
  VLOG(20) << " " << __FUNCTION__ << "()"
           << " full_path = " << full_path.value()
           << DebugString(true);
  DCHECK(!full_path.empty());
  full_path_ = full_path;
}

void DownloadItem::TogglePause() {
  DCHECK(state_ == IN_PROGRESS);
  download_manager_->PauseDownload(id_, !is_paused_);
  is_paused_ = !is_paused_;
  UpdateObservers();
}

void DownloadItem::OnNameFinalized() {
  name_finalized_ = true;

  // The download file is meant to be completed if both the filename is
  // finalized and the file data is downloaded. The ordering of these two
  // actions is indeterministic. Thus, if we are still in downloading the
  // file, delay the notification.
  if (state() == DownloadItem::COMPLETE) {
    NotifyObserversDownloadFileCompleted();
    download_manager_->RemoveFromActiveList(id());
  }
}

void DownloadItem::OnSafeDownloadFinished(DownloadFileManager* file_manager) {
  DCHECK_EQ(SAFE, safety_state());
  DCHECK(file_manager);
  if (NeedsRename()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            file_manager, &DownloadFileManager::OnFinalDownloadName,
            id(), GetTargetFilePath(), false,
            make_scoped_refptr(download_manager_)));
    return;
  }

  Finished();
}

void DownloadItem::OnDownloadRenamedToFinalName(const FilePath& full_path) {
  VLOG(20) << " " << __FUNCTION__ << "()"
           << " full_path = " << full_path.value();
  bool needed_rename = NeedsRename();

  Rename(full_path);
  OnNameFinalized();

  if (needed_rename && safety_state() == SAFE) {
    // This was called from OnSafeDownloadFinished; continue to call
    // DownloadFinished.
    Finished();
  }
}

bool DownloadItem::MatchesQuery(const string16& query) const {
  if (query.empty())
    return true;

  DCHECK_EQ(query, l10n_util::ToLower(query));

  string16 url_raw(l10n_util::ToLower(UTF8ToUTF16(url_.spec())));
  if (url_raw.find(query) != string16::npos)
    return true;

  // TODO(phajdan.jr): write a test case for the following code.
  // A good test case would be:
  //   "/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
  //   L"/\x4f60\x597d\x4f60\x597d",
  //   "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD"
  PrefService* prefs = download_manager_->profile()->GetPrefs();
  std::string languages(prefs->GetString(prefs::kAcceptLanguages));
  string16 url_formatted(l10n_util::ToLower(net::FormatUrl(url_, languages)));
  if (url_formatted.find(query) != string16::npos)
    return true;

  string16 path(l10n_util::ToLower(WideToUTF16(full_path().ToWStringHack())));
  if (path.find(query) != std::wstring::npos)
    return true;

  return false;
}

void DownloadItem::SetFileCheckResults(const FilePath& path,
                                       bool is_dangerous,
                                       int path_uniquifier,
                                       bool prompt,
                                       bool is_extension_install,
                                       const FilePath& original_name) {
  VLOG(20) << " " << __FUNCTION__ << "()"
           << " path = \"" << path.value() << "\""
           << " is_dangerous = " << is_dangerous
           << " path_uniquifier = " << path_uniquifier
           << " prompt = " << prompt
           << " is_extension_install = " << is_extension_install
           << " path = \"" << path.value() << "\""
           << " original_name = \"" << original_name.value() << "\""
           << " " << DebugString(true);
  // Make sure the initial file name is set only once.
  DCHECK(full_path_.empty());
  DCHECK(!path.empty());

  full_path_ = path;
  safety_state_ = is_dangerous ? DANGEROUS : SAFE;
  path_uniquifier_ = path_uniquifier;
  save_as_ = prompt;
  is_extension_install_ = is_extension_install;
  target_name_ = original_name;

  if (target_name_.value().empty())
    target_name_ = full_path_.BaseName();
}

FilePath DownloadItem::GetTargetFilePath() const {
  return full_path_.DirName().Append(target_name_);
}

FilePath DownloadItem::GetFileNameToReportUser() const {
  if (path_uniquifier_ > 0) {
    FilePath name(target_name_);
    download_util::AppendNumberToPath(&name, path_uniquifier_);
    return name;
  }
  return target_name_;
}

FilePath DownloadItem::GetUserVerifiedFilePath() const {
  if (safety_state_ == DownloadItem::SAFE)
    return GetTargetFilePath();
  return full_path_;
}

void DownloadItem::Init(bool start_timer) {
  if (target_name_.value().empty())
    target_name_ = full_path_.BaseName();
  if (start_timer)
    StartProgressTimer();
  VLOG(20) << " " << __FUNCTION__ << "() " << DebugString(true);
}

std::string DownloadItem::DebugString(bool verbose) const {
  std::string description =
      base::StringPrintf("{ id_ = %d"
                         " state = %s",
                         id_,
                         DebugDownloadStateString(state()));

  if (verbose) {
    description += base::StringPrintf(
        " db_handle = %" PRId64
        " total_bytes = %" PRId64
        " is_paused = " "%c"
        " is_extension_install = " "%c"
        " is_otr = " "%c"
        " safety_state = " "%s"
        " url = " "\"%s\""
        " target_name_ = \"%" PRFilePath "\""
        " full_path = \"%" PRFilePath "\"",
        db_handle(),
        total_bytes(),
        is_paused() ? 'T' : 'F',
        is_extension_install() ? 'T' : 'F',
        is_otr() ? 'T' : 'F',
        DebugSafetyStateString(safety_state()),
        url().spec().c_str(),
        target_name_.value().c_str(),
        full_path().value().c_str());
  } else {
    description += base::StringPrintf(" url = \"%s\"", url().spec().c_str());
  }

  description += " }";

  return description;
}
