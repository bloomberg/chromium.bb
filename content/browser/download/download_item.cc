// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_item.h"

#include <vector>

#include "base/bind.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "net/base/net_util.h"

using content::BrowserThread;

// A DownloadItem normally goes through the following states:
//      * Created (when download starts)
//      * Made visible to consumers (e.g. Javascript) after the
//        destination file has been determined.
//      * Entered into the history database.
//      * Made visible in the download shelf.
//      * All data is saved.  Note that the actual data download occurs
//        in parallel with the above steps, but until those steps are
//        complete, completion of the data download will be ignored.
//      * Download file is renamed to its final name, and possibly
//        auto-opened.
// TODO(rdsmith): This progress should be reflected in
// DownloadItem::DownloadState and a state transition table/state diagram.
//
// TODO(rdsmith): This description should be updated to reflect the cancel
// pathways.

namespace {

static void DeleteDownloadedFile(const FilePath& path) {
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
    case DownloadItem::INTERRUPTED:
      return "INTERRUPTED";
    default:
      NOTREACHED() << "Unknown download state " << state;
      return "unknown";
  };
}

// Classes to null out request handle calls (for SavePage DownloadItems, which
// may have, e.g., Cancel() called on them without it doing anything)
// and to DCHECK on them (for history DownloadItems, which should never have
// any operation that implies an off-thread component, since they don't
// have any).
class NullDownloadRequestHandle : public DownloadRequestHandleInterface {
 public:
  NullDownloadRequestHandle() {}

  // DownloadRequestHandleInterface calls
  virtual TabContents* GetTabContents() const OVERRIDE {
    return NULL;
  }
  virtual DownloadManager* GetDownloadManager() const OVERRIDE {
    return NULL;
  }
  virtual void PauseRequest() const OVERRIDE {}
  virtual void ResumeRequest() const OVERRIDE {}
  virtual void CancelRequest() const OVERRIDE {}
  virtual std::string DebugString() const OVERRIDE {
    return "Null DownloadRequestHandle";
  }
};


}  // namespace

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressively getting
// more negative.
// static
const int DownloadItem::kUninitializedHandle = 0;

const char DownloadItem::kEmptyFileHash[] = "";

// Constructor for reading from the history service.
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const DownloadPersistentStoreInfo& info)
    : download_id_(download_manager->GetNextId()),
      full_path_(info.path),
      url_chain_(1, info.url),
      referrer_url_(info.referrer_url),
      total_bytes_(info.total_bytes),
      received_bytes_(info.received_bytes),
      start_tick_(base::TimeTicks()),
      state_(static_cast<DownloadState>(info.state)),
      start_time_(info.start_time),
      end_time_(info.end_time),
      db_handle_(info.db_handle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_otr_(false),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(info.opened),
      open_enabled_(true),
      delegate_delayed_complete_(false) {
  if (IsInProgress())
    state_ = CANCELLED;
  if (IsComplete())
    all_data_saved_ = true;
  Init(false /* not actively downloading */);
}

// Constructing for a regular download:
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const DownloadCreateInfo& info,
                           DownloadRequestHandleInterface* request_handle,
                           bool is_otr)
    : state_info_(info.original_name, info.save_info.file_path,
                  info.has_user_gesture, info.transition_type,
                  info.prompt_user_for_save_location, info.path_uniquifier,
                  DownloadStateInfo::NOT_DANGEROUS),
      request_handle_(request_handle),
      download_id_(info.download_id),
      full_path_(info.path),
      url_chain_(info.url_chain),
      referrer_url_(info.referrer_url),
      suggested_filename_(UTF16ToUTF8(info.save_info.suggested_name)),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      referrer_charset_(info.referrer_charset),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(info.start_time),
      db_handle_(DownloadItem::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_otr_(is_otr),
      is_temporary_(!info.save_info.file_path.empty()),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false) {
  Init(true /* actively downloading */);
}

// Constructing for the "Save Page As..." feature:
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const FilePath& path,
                           const GURL& url,
                           bool is_otr,
                           DownloadId download_id)
    : request_handle_(new NullDownloadRequestHandle()),
      download_id_(download_id),
      full_path_(path),
      url_chain_(1, url),
      referrer_url_(GURL()),
      total_bytes_(0),
      received_bytes_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(base::Time::Now()),
      db_handle_(DownloadItem::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_otr_(is_otr),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false) {
  Init(true /* actively downloading */);
}

DownloadItem::~DownloadItem() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TransitionTo(REMOVING);
  download_manager_->AssertQueueStateConsistent(this);
}

void DownloadItem::AddObserver(Observer* observer) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.AddObserver(observer);
}

void DownloadItem::RemoveObserver(Observer* observer) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.RemoveObserver(observer);
}

void DownloadItem::UpdateObservers() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

bool DownloadItem::CanShowInFolder() {
  return !IsCancelled() && !file_externally_removed_;
}

bool DownloadItem::CanOpenDownload() {
  return !file_externally_removed_;
}

bool DownloadItem::ShouldOpenFileBasedOnExtension() {
  return download_manager_->delegate()->ShouldOpenFileBasedOnExtension(
      GetUserVerifiedFilePath());
}

void DownloadItem::OpenDownload() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsPartialDownload()) {
    open_when_complete_ = !open_when_complete_;
    return;
  }

  if (!IsComplete() || file_externally_removed_)
    return;

  // Ideally, we want to detect errors in opening and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the open.
  download_manager_->CheckForFileRemoval(this);
  download_stats::RecordOpen(end_time(), !opened());
  opened_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
  download_manager_->MarkDownloadOpened(this);

  // For testing: If download opening is disabled on this item,
  // make the rest of the routine a no-op.
  if (!open_enabled_)
    return;

  content::GetContentClient()->browser()->OpenItem(full_path());
}

void DownloadItem::ShowDownloadInShell() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::GetContentClient()->browser()->ShowItemInFolder(full_path());
}

void DownloadItem::DangerousDownloadValidated() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(DANGEROUS, safety_state());

  UMA_HISTOGRAM_ENUMERATION("Download.DangerousDownloadValidated",
                            GetDangerType(),
                            DownloadStateInfo::DANGEROUS_TYPE_MAX);

  safety_state_ = DANGEROUS_BUT_VALIDATED;
  UpdateObservers();

  download_manager_->MaybeCompleteDownload(this);
}

void DownloadItem::UpdateSize(int64 bytes_so_far) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;
}

// Updates from the download thread may have been posted while this download
// was being cancelled in the UI thread, so we'll accept them unless we're
// complete.
void DownloadItem::Update(int64 bytes_so_far) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsInProgress()) {
    NOTREACHED();
    return;
  }
  UpdateSize(bytes_so_far);
  UpdateObservers();
}

// Triggered by a user action.
void DownloadItem::Cancel(bool user_cancel) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  last_reason_ = user_cancel ?
      DOWNLOAD_INTERRUPT_REASON_USER_CANCELED :
      DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  if (!IsPartialDownload()) {
    // Small downloads might be complete before this method has
    // a chance to run.
    return;
  }

  download_stats::RecordDownloadCount(download_stats::CANCELLED_COUNT);

  TransitionTo(CANCELLED);
  if (user_cancel)
    download_manager_->DownloadCancelledInternal(this);
}

void DownloadItem::MarkAsComplete() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE);
}

void DownloadItem::DelayedDownloadOpened() {
  auto_opened_ = true;
  Completed();
}

void DownloadItem::OnAllDataSaved(int64 size, const std::string& final_hash) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!all_data_saved_);
  all_data_saved_ = true;
  UpdateSize(size);
  hash_ = final_hash;
}

void DownloadItem::OnDownloadedFileRemoved() {
  file_externally_removed_ = true;
  UpdateObservers();
}

void DownloadItem::Completed() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "() " << DebugString(false);

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE);
  download_manager_->DownloadCompleted(id());
  download_stats::RecordDownloadCompleted(start_tick_, received_bytes_);

  if (auto_opened_) {
    // If it was already handled by the delegate, do nothing.
  } else if (open_when_complete() ||
             ShouldOpenFileBasedOnExtension() ||
             is_temporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!is_temporary())
      OpenDownload();

    auto_opened_ = true;
    UpdateObservers();
  }
}

void DownloadItem::TransitionTo(DownloadState new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;
  UpdateObservers();
}

void DownloadItem::UpdateSafetyState() {
  SafetyState updated_value = state_info_.IsDangerous() ?
      DownloadItem::DANGEROUS : DownloadItem::SAFE;
  if (updated_value != safety_state_) {
    safety_state_ = updated_value;
    UpdateObservers();
  }
}

void DownloadItem::UpdateTarget() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_info_.target_name.value().empty())
    state_info_.target_name = full_path_.BaseName();
}

void DownloadItem::Interrupted(int64 size, InterruptReason reason) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsInProgress())
    return;

  last_reason_ = reason;
  UpdateSize(size);
  download_stats::RecordDownloadInterrupted(reason,
                                            received_bytes_,
                                            total_bytes_);
  TransitionTo(INTERRUPTED);
}

void DownloadItem::Delete(DeleteReason reason) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (reason) {
    case DELETE_DUE_TO_USER_DISCARD:
      UMA_HISTOGRAM_ENUMERATION("Download.UserDiscard", GetDangerType(),
                                DownloadStateInfo::DANGEROUS_TYPE_MAX);
      break;
    case DELETE_DUE_TO_BROWSER_SHUTDOWN:
      UMA_HISTOGRAM_ENUMERATION("Download.Discard", GetDangerType(),
                                DownloadStateInfo::DANGEROUS_TYPE_MAX);
      break;
    default:
      NOTREACHED();
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DeleteDownloadedFile, full_path_));
  Remove();
  // We have now been deleted.
}

void DownloadItem::Remove() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  download_manager_->AssertQueueStateConsistent(this);
  Cancel(true);
  download_manager_->AssertQueueStateConsistent(this);

  TransitionTo(REMOVING);
  download_manager_->RemoveDownload(db_handle_);
  // We have now been deleted.
}

bool DownloadItem::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining = base::TimeDelta::FromSeconds(
      (total_bytes_ - received_bytes_) / speed);
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
  // If the delegate is delaying completion of the download, then we have no
  // idea how long it will take.
  if (delegate_delayed_complete_ || total_bytes_ <= 0)
    return -1;

  return static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
}

void DownloadItem::OnPathDetermined(const FilePath& path) {
  full_path_ = path;
  UpdateTarget();
}

void DownloadItem::Rename(const FilePath& full_path) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " full_path = \"" << full_path.value() << "\""
           << " " << DebugString(true);
  DCHECK(!full_path.empty());
  full_path_ = full_path;
}

void DownloadItem::TogglePause() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(IsInProgress());
  if (is_paused_)
    request_handle_->ResumeRequest();
  else
    request_handle_->PauseRequest();
  is_paused_ = !is_paused_;
  UpdateObservers();
}

void DownloadItem::OnDownloadCompleting(DownloadFileManager* file_manager) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " needs rename = " << NeedsRename()
           << " " << DebugString(true);
  DCHECK_NE(DANGEROUS, safety_state());
  DCHECK(file_manager);

  if (NeedsRename()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::RenameCompletingDownloadFile,
                   file_manager, global_id(),
                   GetTargetFilePath(), safety_state() == SAFE));
    return;
  }

  Completed();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DownloadFileManager::CompleteDownload,
                                     file_manager, global_id()));
}

void DownloadItem::OnDownloadRenamedToFinalName(const FilePath& full_path) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " full_path = \"" << full_path.value() << "\""
           << " needed rename = " << NeedsRename()
           << " " << DebugString(false);
  DCHECK(NeedsRename());

  Rename(full_path);

  if (download_manager_->delegate()->ShouldOpenDownload(this)) {
    Completed();
  } else {
    delegate_delayed_complete_ = true;
  }
}

bool DownloadItem::MatchesQuery(const string16& query) const {
  if (query.empty())
    return true;

  DCHECK_EQ(query, base::i18n::ToLower(query));

  string16 url_raw(UTF8ToUTF16(GetURL().spec()));
  if (base::i18n::StringSearchIgnoringCaseAndAccents(query, url_raw))
    return true;

  // TODO(phajdan.jr): write a test case for the following code.
  // A good test case would be:
  //   "/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
  //   L"/\x4f60\x597d\x4f60\x597d",
  //   "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD"
  std::string languages;
  TabContents* tab = GetTabContents();
  if (tab)
    languages = content::GetContentClient()->browser()->GetAcceptLangs(tab);
  string16 url_formatted(net::FormatUrl(GetURL(), languages));
  if (base::i18n::StringSearchIgnoringCaseAndAccents(query, url_formatted))
    return true;

  string16 path(full_path().LossyDisplayName());
  return base::i18n::StringSearchIgnoringCaseAndAccents(query, path);
}

void DownloadItem::SetFileCheckResults(const DownloadStateInfo& state) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << " " << __FUNCTION__ << "()" << " this = " << DebugString(true);
  state_info_ = state;
  VLOG(20) << " " << __FUNCTION__ << "()" << " this = " << DebugString(true);

  UpdateSafetyState();
}

DownloadStateInfo::DangerType DownloadItem::GetDangerType() const {
  return state_info_.danger;
}

bool DownloadItem::IsDangerous() const {
  return state_info_.IsDangerous();
}

void DownloadItem::MarkFileDangerous() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  state_info_.danger = DownloadStateInfo::DANGEROUS_FILE;
  UpdateSafetyState();
}

void DownloadItem::MarkUrlDangerous() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  state_info_.danger = DownloadStateInfo::DANGEROUS_URL;
  UpdateSafetyState();
}

void DownloadItem::MarkContentDangerous() {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  state_info_.danger = DownloadStateInfo::DANGEROUS_CONTENT;
  UpdateSafetyState();
}

DownloadPersistentStoreInfo DownloadItem::GetPersistentStoreInfo() const {
  return DownloadPersistentStoreInfo(full_path(),
                                     GetURL(),
                                     referrer_url(),
                                     start_time(),
                                     end_time(),
                                     received_bytes(),
                                     total_bytes(),
                                     state(),
                                     db_handle(),
                                     opened());
}

TabContents* DownloadItem::GetTabContents() const {
  // TODO(rdsmith): Remove null check after removing GetTabContents() from
  // paths that might be used by DownloadItems created from history import.
  // Currently such items have null request_handle_s, where other items
  // (regular and SavePackage downloads) have actual objects off the pointer.
  if (request_handle_.get())
    return request_handle_->GetTabContents();
  return NULL;
}

FilePath DownloadItem::GetTargetFilePath() const {
  return full_path_.DirName().Append(state_info_.target_name);
}

FilePath DownloadItem::GetFileNameToReportUser() const {
  if (state_info_.path_uniquifier > 0) {
    FilePath name(state_info_.target_name);
    DownloadFile::AppendNumberToPath(&name, state_info_.path_uniquifier);
    return name;
  }
  return state_info_.target_name;
}

FilePath DownloadItem::GetUserVerifiedFilePath() const {
  return (safety_state_ == DownloadItem::SAFE) ?
      GetTargetFilePath() : full_path_;
}

void DownloadItem::OffThreadCancel(DownloadFileManager* file_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  request_handle_->CancelRequest();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DownloadFileManager::CancelDownload,
                                     file_manager, global_id()));
}

void DownloadItem::Init(bool active) {
  // TODO(rdsmith): Change to DCHECK after http://crbug.com/85408 resolved.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UpdateTarget();
  if (active) {
    download_stats::RecordDownloadCount(download_stats::START_COUNT);
  }
  VLOG(20) << __FUNCTION__ << "() " << DebugString(true);
}

// TODO(ahendrickson) -- Move |INTERRUPTED| from |IsCancelled()| to
// |IsPartialDownload()|, when resuming interrupted downloads is implemented.
bool DownloadItem::IsPartialDownload() const {
  return (state_ == IN_PROGRESS);
}

bool DownloadItem::IsInProgress() const {
  return (state_ == IN_PROGRESS);
}

bool DownloadItem::IsCancelled() const {
  return (state_ == CANCELLED) ||
         (state_ == INTERRUPTED);
}

bool DownloadItem::IsInterrupted() const {
  return (state_ == INTERRUPTED);
}

bool DownloadItem::IsComplete() const {
  return (state_ == COMPLETE);
}

const GURL& DownloadItem::GetURL() const {
  return url_chain_.empty() ?
             GURL::EmptyGURL() : url_chain_.back();
}

std::string DownloadItem::DebugString(bool verbose) const {
  std::string description =
      base::StringPrintf("{ id = %d"
                         " state = %s",
                         download_id_.local(),
                         DebugDownloadStateString(state()));

  // Construct a string of the URL chain.
  std::string url_list("<none>");
  if (!url_chain_.empty()) {
    std::vector<GURL>::const_iterator iter = url_chain_.begin();
    std::vector<GURL>::const_iterator last = url_chain_.end();
    url_list = (*iter).spec();
    ++iter;
    for ( ; verbose && (iter != last); ++iter) {
      url_list += " ->\n\t";
      const GURL& next_url = *iter;
      url_list += next_url.spec();
    }
  }

  if (verbose) {
    description += base::StringPrintf(
        " db_handle = %" PRId64
        " total_bytes = %" PRId64
        " received_bytes = %" PRId64
        " is_paused = %c"
        " is_otr = %c"
        " safety_state = %s"
        " url_chain = \n\t\"%s\"\n\t"
        " target_name = \"%" PRFilePath "\""
        " full_path = \"%" PRFilePath "\"",
        db_handle(),
        total_bytes(),
        received_bytes(),
        is_paused() ? 'T' : 'F',
        is_otr() ? 'T' : 'F',
        DebugSafetyStateString(safety_state()),
        url_list.c_str(),
        state_info_.target_name.value().c_str(),
        full_path().value().c_str());
  } else {
    description += base::StringPrintf(" url = \"%s\"", url_list.c_str());
  }

  description += " }";

  return description;
}
