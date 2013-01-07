// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File method ordering: Methods in this file are in the same order as
// in download_item_impl.h, with the following exception: The public
// interface Start is placed in chronological order with the other
// (private) routines that together define a DownloadItem's state
// transitions as the download progresses.  See "Download progression
// cascade" later in this file.

// A regular DownloadItem (created for a download in this session of the
// browser) normally goes through the following states:
//      * Created (when download starts)
//      * Destination filename determined
//      * Entered into the history database.
//      * Made visible in the download shelf.
//      * All the data is saved.  Note that the actual data download occurs
//        in parallel with the above steps, but until those steps are
//        complete, the state of the data save will be ignored.
//      * Download file is renamed to its final name, and possibly
//        auto-opened.

#include "content/browser/download/download_item_impl.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/net_util.h"

namespace content {
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

// Classes to null out request handle calls (for SavePage DownloadItems, which
// may have, e.g., Cancel() called on them without it doing anything)
// and to DCHECK on them (for history DownloadItems, which should never have
// any operation that implies an off-thread component, since they don't
// have any).
class NullDownloadRequestHandle : public DownloadRequestHandleInterface {
 public:
  NullDownloadRequestHandle() {}

  // DownloadRequestHandleInterface calls
  virtual WebContents* GetWebContents() const OVERRIDE {
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

// Wrapper around DownloadFile::Detach and DownloadFile::Cancel that
// takes ownership of the DownloadFile and hence implicitly destroys it
// at the end of the function.
static void DownloadFileDetach(scoped_ptr<DownloadFile> download_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  download_file->Detach();
}

static void DownloadFileCancel(scoped_ptr<DownloadFile> download_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  download_file->Cancel();
}

}  // namespace

const char DownloadItem::kEmptyFileHash[] = "";

// Constructor for reading from the history service.
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   DownloadId download_id,
                                   const FilePath& path,
                                   const GURL& url,
                                   const GURL& referrer_url,
                                   const base::Time& start_time,
                                   const base::Time& end_time,
                                   int64 received_bytes,
                                   int64 total_bytes,
                                   DownloadItem::DownloadState state,
                                   bool opened,
                                   const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(false),
      download_id_(download_id),
      current_path_(path),
      target_path_(path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(1, url),
      referrer_url_(referrer_url),
      transition_type_(PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      total_bytes_(total_bytes),
      received_bytes_(received_bytes),
      bytes_per_sec_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks()),
      state_(ExternalToInternalState(state)),
      danger_type_(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(start_time),
      end_time_(end_time),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(opened),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  if (state_ == IN_PROGRESS_INTERNAL)
    state_ = CANCELLED_INTERNAL;
  if (state_ == COMPLETE_INTERNAL)
    all_data_saved_ = true;
  Init(false /* not actively downloading */, SRC_HISTORY_IMPORT);
}

// Constructing for a regular download:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const DownloadCreateInfo& info,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(false),
      request_handle_(request_handle.Pass()),
      download_id_(info.download_id),
      target_disposition_(
          (info.save_info->prompt_for_save_location) ?
              TARGET_DISPOSITION_PROMPT : TARGET_DISPOSITION_OVERWRITE),
      url_chain_(info.url_chain),
      referrer_url_(info.referrer_url),
      suggested_filename_(UTF16ToUTF8(info.save_info->suggested_name)),
      forced_file_path_(info.save_info->file_path),
      transition_type_(info.transition_type),
      has_user_gesture_(info.has_user_gesture),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      remote_address_(info.remote_address),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      danger_type_(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(info.start_time),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_temporary_(!info.save_info->file_path.empty()),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_NEW_DOWNLOAD);

  // Link the event sources.
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_URL_REQUEST,
      info.request_bound_net_log.source().ToEventParametersCallback());

  info.request_bound_net_log.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_STARTED,
      bound_net_log_.source().ToEventParametersCallback());
}

// Constructing for the "Save Page As..." feature:
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   const FilePath& path,
                                   const GURL& url,
                                   DownloadId download_id,
                                   const std::string& mime_type,
                                   const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(true),
      request_handle_(new NullDownloadRequestHandle()),
      download_id_(download_id),
      current_path_(path),
      target_path_(path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(1, url),
      referrer_url_(GURL()),
      transition_type_(PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      mime_type_(mime_type),
      original_mime_type_(mime_type),
      total_bytes_(0),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      danger_type_(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(base::Time::Now()),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_SAVE_PAGE_AS);
}

DownloadItemImpl::~DownloadItemImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should always have been nuked before now, at worst in
  // DownloadManager shutdown.
  DCHECK(!download_file_.get());

  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadDestroyed(this));
  delegate_->AssertStateConsistent(this);
  delegate_->Detach();
}

void DownloadItemImpl::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.AddObserver(observer);
}

void DownloadItemImpl::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.RemoveObserver(observer);
}

void DownloadItemImpl::UpdateObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

void DownloadItemImpl::DangerousDownloadValidated() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(IN_PROGRESS, GetState());
  DCHECK_EQ(DANGEROUS, GetSafetyState());

  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  if (GetState() != IN_PROGRESS)
    return;

  UMA_HISTOGRAM_ENUMERATION("Download.DangerousDownloadValidated",
                            GetDangerType(),
                            DOWNLOAD_DANGER_TYPE_MAX);

  safety_state_ = DANGEROUS_BUT_VALIDATED;

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
      base::Bind(&ItemCheckedNetLogCallback,
                 GetDangerType(), GetSafetyState()));

  UpdateObservers();

  MaybeCompleteDownload();
}

void DownloadItemImpl::Pause() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore irrelevant states.
  if (state_ != IN_PROGRESS_INTERNAL || is_paused_)
    return;

  request_handle_->PauseRequest();
  is_paused_ = true;
  UpdateObservers();
}

void DownloadItemImpl::Resume() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore irrelevant states.
  if (state_ != IN_PROGRESS_INTERNAL || !is_paused_)
    return;

  request_handle_->ResumeRequest();
  is_paused_ = false;
  UpdateObservers();
}

void DownloadItemImpl::Cancel(bool user_cancel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  last_reason_ = user_cancel ?
      DOWNLOAD_INTERRUPT_REASON_USER_CANCELED :
      DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  if (state_ != IN_PROGRESS_INTERNAL) {
    // Small downloads might be complete before this method has
    // a chance to run.
    return;
  }

  RecordDownloadCount(CANCELLED_COUNT);

  TransitionTo(CANCELLED_INTERNAL);

  CancelDownloadFile();

  // Cancel the originating URL request.
  request_handle_->CancelRequest();
}

void DownloadItemImpl::Delete(DeleteReason reason) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (reason) {
    case DELETE_DUE_TO_USER_DISCARD:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.UserDiscard", GetDangerType(),
          DOWNLOAD_DANGER_TYPE_MAX);
      break;
    case DELETE_DUE_TO_BROWSER_SHUTDOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.Discard", GetDangerType(),
          DOWNLOAD_DANGER_TYPE_MAX);
      break;
    default:
      NOTREACHED();
  }

  // Delete the file if it exists and is not owned by a DownloadFile object.
  // (In the latter case the DownloadFile object will delete it on cancel.)
  if (!current_path_.empty() && download_file_.get() == NULL) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&DeleteDownloadedFile, current_path_));
  }
  Remove();
  // We have now been deleted.
}

void DownloadItemImpl::Remove() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  delegate_->AssertStateConsistent(this);
  Cancel(true);
  delegate_->AssertStateConsistent(this);

  NotifyRemoved();
  delegate_->DownloadRemoved(this);
  // We have now been deleted.
}

void DownloadItemImpl::OpenDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == IN_PROGRESS_INTERNAL) {
    // We don't honor the open_when_complete_ flag for temporary
    // downloads. Don't set it because it shows up in the UI.
    if (!IsTemporary())
      open_when_complete_ = !open_when_complete_;
    return;
  }

  if (state_ != COMPLETE_INTERNAL || file_externally_removed_)
    return;

  // Ideally, we want to detect errors in opening and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the open.
  delegate_->CheckForFileRemoval(this);
  RecordOpen(GetEndTime(), !GetOpened());
  opened_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
  delegate_->DownloadOpened(this);

  // For testing: If download opening is disabled on this item,
  // make the rest of the routine a no-op.
  if (!open_enabled_)
    return;

  GetContentClient()->browser()->OpenItem(GetFullPath());
}

void DownloadItemImpl::ShowDownloadInShell() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetContentClient()->browser()->ShowItemInFolder(GetFullPath());
}

int32 DownloadItemImpl::GetId() const {
  return download_id_.local();
}

DownloadId DownloadItemImpl::GetGlobalId() const {
  return download_id_;
}

DownloadItem::DownloadState DownloadItemImpl::GetState() const {
  return InternalToExternalState(state_);
}

DownloadInterruptReason DownloadItemImpl::GetLastReason() const {
  return last_reason_;
}

bool DownloadItemImpl::IsPaused() const {
  return is_paused_;
}

bool DownloadItemImpl::IsTemporary() const {
  return is_temporary_;
}

// TODO(ahendrickson) -- Move |INTERRUPTED| from |IsCancelled()| to
// |IsPartialDownload()|, when resuming interrupted downloads is implemented.
bool DownloadItemImpl::IsPartialDownload() const {
  return InternalToExternalState(state_) == IN_PROGRESS;
}

bool DownloadItemImpl::IsInProgress() const {
  return InternalToExternalState(state_) == IN_PROGRESS;
}

bool DownloadItemImpl::IsCancelled() const {
  DownloadState external_state = InternalToExternalState(state_);
  return  external_state == CANCELLED || external_state == INTERRUPTED;
}

bool DownloadItemImpl::IsInterrupted() const {
  return InternalToExternalState(state_) == INTERRUPTED;
}

bool DownloadItemImpl::IsComplete() const {
  return InternalToExternalState(state_) == COMPLETE;
}

const GURL& DownloadItemImpl::GetURL() const {
  return url_chain_.empty() ?
             GURL::EmptyGURL() : url_chain_.back();
}

const std::vector<GURL>& DownloadItemImpl::GetUrlChain() const {
  return url_chain_;
}

const GURL& DownloadItemImpl::GetOriginalUrl() const {
  return url_chain_.front();
}

const GURL& DownloadItemImpl::GetReferrerUrl() const {
  return referrer_url_;
}

std::string DownloadItemImpl::GetSuggestedFilename() const {
  return suggested_filename_;
}

std::string DownloadItemImpl::GetContentDisposition() const {
  return content_disposition_;
}

std::string DownloadItemImpl::GetMimeType() const {
  return mime_type_;
}

std::string DownloadItemImpl::GetOriginalMimeType() const {
  return original_mime_type_;
}

std::string DownloadItemImpl::GetRemoteAddress() const {
  return remote_address_;
}

bool DownloadItemImpl::HasUserGesture() const {
  return has_user_gesture_;
};

PageTransition DownloadItemImpl::GetTransitionType() const {
  return transition_type_;
};

const std::string& DownloadItemImpl::GetLastModifiedTime() const {
  return last_modified_time_;
}

const std::string& DownloadItemImpl::GetETag() const {
  return etag_;
}

bool DownloadItemImpl::IsSavePackageDownload() const {
  return is_save_package_download_;
}

const FilePath& DownloadItemImpl::GetFullPath() const {
  return current_path_;
}

const FilePath& DownloadItemImpl::GetTargetFilePath() const {
  return target_path_;
}

const FilePath& DownloadItemImpl::GetForcedFilePath() const {
  // TODO(asanka): Get rid of GetForcedFilePath(). We should instead just
  // require that clients respect GetTargetFilePath() if it is already set.
  return forced_file_path_;
}

FilePath DownloadItemImpl::GetUserVerifiedFilePath() const {
  return (safety_state_ == DownloadItem::SAFE) ?
      GetTargetFilePath() : GetFullPath();
}

FilePath DownloadItemImpl::GetFileNameToReportUser() const {
  if (!display_name_.empty())
    return display_name_;
  return target_path_.BaseName();
}

DownloadItem::TargetDisposition DownloadItemImpl::GetTargetDisposition() const {
  return target_disposition_;
}

const std::string& DownloadItemImpl::GetHash() const {
  return hash_;
}

const std::string& DownloadItemImpl::GetHashState() const {
  return hash_state_;
}

bool DownloadItemImpl::GetFileExternallyRemoved() const {
  return file_externally_removed_;
}

// TODO(asanka): Unify GetSafetyState() and IsDangerous().
DownloadItem::SafetyState DownloadItemImpl::GetSafetyState() const {
  return safety_state_;
}

bool DownloadItemImpl::IsDangerous() const {
  // TODO(noelutz): At this point only the windows views UI supports
  // warnings based on dangerous content.
#ifdef OS_WIN
  return (danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
#else
  return (danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
#endif
}

DownloadDangerType DownloadItemImpl::GetDangerType() const {
  return danger_type_;
}

bool DownloadItemImpl::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining = base::TimeDelta::FromSeconds(
      (total_bytes_ - received_bytes_) / speed);
  return true;
}

int64 DownloadItemImpl::CurrentSpeed() const {
  if (is_paused_)
    return 0;
  return bytes_per_sec_;
}

int DownloadItemImpl::PercentComplete() const {
  // If the delegate is delaying completion of the download, then we have no
  // idea how long it will take.
  if (delegate_delayed_complete_ || total_bytes_ <= 0)
    return -1;

  return static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
}

bool DownloadItemImpl::AllDataSaved() const {
  return all_data_saved_;
}

int64 DownloadItemImpl::GetTotalBytes() const {
  return total_bytes_;
}

int64 DownloadItemImpl::GetReceivedBytes() const {
  return received_bytes_;
}

base::Time DownloadItemImpl::GetStartTime() const {
  return start_time_;
}

base::Time DownloadItemImpl::GetEndTime() const {
  return end_time_;
}

bool DownloadItemImpl::CanShowInFolder() {
  // A download can be shown in the folder if the downloaded file is in a known
  // location.
  return CanOpenDownload() && !GetFullPath().empty();
}

bool DownloadItemImpl::CanOpenDownload() {
  // We can open the file or mark it for opening on completion if the download
  // is expected to complete successfully. Exclude temporary downloads, since
  // they aren't owned by the download system.
  return (IsInProgress() || IsComplete()) && !IsTemporary() &&
      !file_externally_removed_;
}

bool DownloadItemImpl::ShouldOpenFileBasedOnExtension() {
  return delegate_->ShouldOpenFileBasedOnExtension(GetUserVerifiedFilePath());
}

bool DownloadItemImpl::GetOpenWhenComplete() const {
  return open_when_complete_;
}

bool DownloadItemImpl::GetAutoOpened() {
  return auto_opened_;
}

bool DownloadItemImpl::GetOpened() const {
  return opened_;
}

BrowserContext* DownloadItemImpl::GetBrowserContext() const {
  return delegate_->GetBrowserContext();
}

WebContents* DownloadItemImpl::GetWebContents() const {
  // TODO(rdsmith): Remove null check after removing GetWebContents() from
  // paths that might be used by DownloadItems created from history import.
  // Currently such items have null request_handle_s, where other items
  // (regular and SavePackage downloads) have actual objects off the pointer.
  if (request_handle_.get())
    return request_handle_->GetWebContents();
  return NULL;
}

void DownloadItemImpl::OnContentCheckCompleted(DownloadDangerType danger_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(AllDataSaved());
  VLOG(20) << __FUNCTION__ << " danger_type=" << danger_type
           << " download=" << DebugString(true);
  SetDangerType(danger_type);
  UpdateObservers();
}

void DownloadItemImpl::SetOpenWhenComplete(bool open) {
  open_when_complete_ = open;
}

void DownloadItemImpl::SetIsTemporary(bool temporary) {
  is_temporary_ = temporary;
}

void DownloadItemImpl::SetOpened(bool opened) {
  opened_ = opened;
}

void DownloadItemImpl::SetDisplayName(const FilePath& name) {
  display_name_ = name;
}

std::string DownloadItemImpl::DebugString(bool verbose) const {
  std::string description =
      base::StringPrintf("{ id = %d"
                         " state = %s",
                         download_id_.local(),
                         DebugDownloadStateString(state_));

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
        " total = %" PRId64
        " received = %" PRId64
        " reason = %s"
        " paused = %c"
        " safety = %s"
        " last_modified = '%s'"
        " etag = '%s'"
        " url_chain = \n\t\"%s\"\n\t"
        " full_path = \"%" PRFilePath "\""
        " target_path = \"%" PRFilePath "\""
        " has download file = %s",
        GetTotalBytes(),
        GetReceivedBytes(),
        InterruptReasonDebugString(last_reason_).c_str(),
        IsPaused() ? 'T' : 'F',
        DebugSafetyStateString(GetSafetyState()),
        GetLastModifiedTime().c_str(),
        GetETag().c_str(),
        url_list.c_str(),
        GetFullPath().value().c_str(),
        GetTargetFilePath().value().c_str(),
        download_file_.get() ? "true" : "false");
  } else {
    description += base::StringPrintf(" url = \"%s\"", url_list.c_str());
  }

  description += " }";

  return description;
}

void DownloadItemImpl::MockDownloadOpenForTesting() {
  open_enabled_ = false;
}

void DownloadItemImpl::NotifyRemoved() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadRemoved(this));
}

void DownloadItemImpl::OnDownloadedFileRemoved() {
  file_externally_removed_ = true;
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);
  UpdateObservers();
}

base::WeakPtr<DownloadDestinationObserver>
DownloadItemImpl::DestinationObserverAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DownloadItemImpl::SetTotalBytes(int64 total_bytes) {
  total_bytes_ = total_bytes;
}

// Updates from the download thread may have been posted while this download
// was being cancelled in the UI thread, so we'll accept them unless we're
// complete.
void DownloadItemImpl::UpdateProgress(int64 bytes_so_far,
                                      int64 bytes_per_sec,
                                      const std::string& hash_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << " so_far=" << bytes_so_far
           << " per_sec=" << bytes_per_sec << " download=" << DebugString(true);

  if (state_ != IN_PROGRESS_INTERNAL) {
    // Ignore if we're no longer in-progress.  This can happen if we race a
    // Cancel on the UI thread with an update on the FILE thread.
    //
    // TODO(rdsmith): Arguably we should let this go through, as this means
    // the download really did get further than we know before it was
    // cancelled.  But the gain isn't very large, and the code is more
    // fragile if it has to support in progress updates in a non-in-progress
    // state.  This issue should be readdressed when we revamp performance
    // reporting.
    return;
  }
  bytes_per_sec_ = bytes_per_sec;
  hash_state_ = hash_state;
  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;

  if (bound_net_log_.IsLoggingAllEvents()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_UPDATED,
        net::NetLog::Int64Callback("bytes_so_far", received_bytes_));
  }

  UpdateObservers();
}

void DownloadItemImpl::OnAllDataSaved(const std::string& final_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK(!all_data_saved_);
  all_data_saved_ = true;
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  // Store final hash and null out intermediate serialized hash state.
  hash_ = final_hash;
  hash_state_ = "";

  UpdateObservers();
}

void DownloadItemImpl::MarkAsComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);
}
void DownloadItemImpl::DestinationUpdate(int64 bytes_so_far,
                                         int64 bytes_per_sec,
                                         const std::string& hash_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  if (!IsInProgress()) {
    // Ignore if we're no longer in-progress.  This can happen if we race a
    // Cancel on the UI thread with an update on the FILE thread.
    //
    // TODO(rdsmith): Arguably we should let this go through, as this means
    // the download really did get further than we know before it was
    // cancelled.  But the gain isn't very large, and the code is more
    // fragile if it has to support in progress updates in a non-in-progress
    // state.  This issue should be readdressed when we revamp performance
    // reporting.
    return;
  }
  bytes_per_sec_ = bytes_per_sec;
  hash_state_ = hash_state;
  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;

  if (bound_net_log_.IsLoggingAllEvents()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_UPDATED,
        net::NetLog::Int64Callback("bytes_so_far", received_bytes_));
  }

  UpdateObservers();
}

void DownloadItemImpl::DestinationError(DownloadInterruptReason reason) {
  // The DestinationError and Interrupt routines are being kept separate
  // to allow for a future merging of the Cancel and Interrupt routines.
  Interrupt(reason);
}

void DownloadItemImpl::DestinationCompleted(const std::string& final_hash) {
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);
  if (!IsInProgress())
    return;
  OnAllDataSaved(final_hash);
  MaybeCompleteDownload();
}

// **** Download progression cascade

void DownloadItemImpl::Init(bool active,
                            DownloadType download_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (active)
    RecordDownloadCount(START_COUNT);

  if (target_path_.empty())
    target_path_ = current_path_;
  std::string file_name;
  if (download_type == SRC_HISTORY_IMPORT) {
    // target_path_ works for History and Save As versions.
    file_name = target_path_.AsUTF8Unsafe();
  } else {
    // See if it's set programmatically.
    file_name = forced_file_path_.AsUTF8Unsafe();
    // Possibly has a 'download' attribute for the anchor.
    if (file_name.empty())
      file_name = suggested_filename_;
    // From the URL file name.
    if (file_name.empty())
      file_name = GetURL().ExtractFileName();
  }

  base::Callback<base::Value*(net::NetLog::LogLevel)> active_data = base::Bind(
      &ItemActivatedNetLogCallback, this, download_type, &file_name);
  if (active) {
    bound_net_log_.BeginEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE, active_data);
  } else {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE, active_data);
  }

  VLOG(20) << __FUNCTION__ << "() " << DebugString(true);
}

// We're starting the download.
void DownloadItemImpl::Start(scoped_ptr<DownloadFile> file) {
  DCHECK(!download_file_.get());
  DCHECK(file.get());
  download_file_ = file.Pass();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::Initialize,
                 // Safe because we control download file lifetime.
                 base::Unretained(download_file_.get()),
                 base::Bind(&DownloadItemImpl::OnDownloadFileInitialized,
                            weak_ptr_factory_.GetWeakPtr())));
}

void DownloadItemImpl::OnDownloadFileInitialized(
    DownloadInterruptReason result) {
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    Interrupt(result);
    // TODO(rdsmith): It makes no sense to continue along the
    // regular download path after we've gotten an error.  But it's
    // the way the code has historically worked, and this allows us
    // to get the download persisted and observers of the download manager
    // notified, so tests work.  When we execute all side effects of cancel
    // (including queue removal) immedately rather than waiting for
    // persistence we should replace this comment with a "return;".
  }

  delegate_->DetermineDownloadTarget(
      this, base::Bind(&DownloadItemImpl::OnDownloadTargetDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
}

// Called by delegate_ when the download target path has been
// determined.
void DownloadItemImpl::OnDownloadTargetDetermined(
    const FilePath& target_path,
    TargetDisposition disposition,
    DownloadDangerType danger_type,
    const FilePath& intermediate_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the |target_path| is empty, then we consider this download to be
  // canceled.
  if (target_path.empty()) {
    Cancel(true);
    return;
  }

  VLOG(20) << __FUNCTION__ << " " << target_path.value() << " " << disposition
           << " " << danger_type << " " << DebugString(true);

  target_path_ = target_path;
  target_disposition_ = disposition;
  SetDangerType(danger_type);
  // TODO(asanka): SetDangerType() doesn't need to send a notification here.

  // We want the intermediate and target paths to refer to the same directory so
  // that they are both on the same device and subject to same
  // space/permission/availability constraints.
  DCHECK(intermediate_path.DirName() == target_path.DirName());

  if (state_ != IN_PROGRESS_INTERNAL) {
    // If we've been cancelled or interrupted while the target was being
    // determined, continue the cascade with a null name.
    // The error doesn't matter as the cause of download stoppage
    // will already have been recorded and will be retained, but we use
    // whatever was recorded for consistency.
    OnDownloadRenamedToIntermediateName(last_reason_, FilePath());
    return;
  }

  // Rename to intermediate name.
  // TODO(asanka): Skip this rename if AllDataSaved() is true. This avoids a
  //               spurious rename when we can just rename to the final
  //               filename. Unnecessary renames may cause bugs like
  //               http://crbug.com/74187.
  DCHECK(!is_save_package_download_);
  DCHECK(download_file_.get());
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToIntermediateName,
                 weak_ptr_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::RenameAndUniquify,
                 // Safe because we control download file lifetime.
                 base::Unretained(download_file_.get()),
                 intermediate_path, callback));
}

void DownloadItemImpl::OnDownloadRenamedToIntermediateName(
    DownloadInterruptReason reason,
    const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);
  if (DOWNLOAD_INTERRUPT_REASON_NONE != reason)
    Interrupt(reason);
  else
    SetFullPath(full_path);
  delegate_->ShowDownloadInBrowser(this);

  MaybeCompleteDownload();
}

// When SavePackage downloads MHTML to GData (see
// SavePackageFilePickerChromeOS), GData calls MaybeCompleteDownload() like it
// does for non-SavePackage downloads, but SavePackage downloads never satisfy
// IsDownloadReadyForCompletion(). GDataDownloadObserver manually calls
// DownloadItem::UpdateObservers() when the upload completes so that SavePackage
// notices that the upload has completed and runs its normal Finish() pathway.
// MaybeCompleteDownload() is never the mechanism by which SavePackage completes
// downloads. SavePackage always uses its own Finish() to mark downloads
// complete.
void DownloadItemImpl::MaybeCompleteDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_save_package_download_);

  if (!IsDownloadReadyForCompletion(
          base::Bind(&DownloadItemImpl::MaybeCompleteDownload,
                     weak_ptr_factory_.GetWeakPtr())))
    return;

  // TODO(rdsmith): DCHECK that we only pass through this point
  // once per download.  The natural way to do this is by a state
  // transition on the DownloadItem.

  // Confirm we're in the proper set of states to be here;
  // have all data, have a history handle, (validated or safe).
  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK_NE(DownloadItem::DANGEROUS, GetSafetyState());
  DCHECK(all_data_saved_);

  OnDownloadCompleting();
}

// Called by MaybeCompleteDownload() when it has determined that the download
// is ready for completion.
void DownloadItemImpl::OnDownloadCompleting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " " << DebugString(true);
  DCHECK(!GetTargetFilePath().empty());
  DCHECK_NE(DANGEROUS, GetSafetyState());

  // TODO(rdsmith/benjhayden): Remove as part of SavePackage integration.
  if (is_save_package_download_) {
    // Avoid doing anything on the file thread; there's nothing we control
    // there.
    // Strictly speaking, this skips giving the embedder a chance to open
    // the download.  But on a save package download, there's no real
    // concept of opening.
    Completed();
    return;
  }

  DCHECK(download_file_.get());
  // Unilaterally rename; even if it already has the right name,
  // we need theannotation.
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToFinalName,
                 weak_ptr_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::RenameAndAnnotate,
                 base::Unretained(download_file_.get()),
                 GetTargetFilePath(), callback));
}

void DownloadItemImpl::OnDownloadRenamedToFinalName(
    DownloadInterruptReason reason,
    const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_save_package_download_);

  // If a cancel or interrupt hit, we'll cancel the DownloadFile, which
  // will result in deleting the file on the file thread.  So we don't
  // care about the name having been changed.
  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " full_path = \"" << full_path.value() << "\""
           << " " << DebugString(false);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    Interrupt(reason);
    return;
  }

  DCHECK(target_path_ == full_path);

  if (full_path != current_path_) {
    // full_path is now the current and target file path.
    DCHECK(!full_path.empty());
    SetFullPath(full_path);
  }

  // Complete the download and release the DownloadFile.
  DCHECK(download_file_.get());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileDetach, base::Passed(&download_file_)));

  // We're not completely done with the download item yet, but at this
  // point we're committed to complete the download.  Cancels (or Interrupts,
  // though it's not clear how they could happen) after this point will be
  // ignored.
  TransitionTo(COMPLETING_INTERNAL);

  if (delegate_->ShouldOpenDownload(
          this, base::Bind(&DownloadItemImpl::DelayedDownloadOpened,
                           weak_ptr_factory_.GetWeakPtr()))) {
    Completed();
  } else {
    delegate_delayed_complete_ = true;
  }
}

void DownloadItemImpl::DelayedDownloadOpened(bool auto_opened) {
  auto_opened_ = auto_opened;
  Completed();
}

void DownloadItemImpl::Completed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "() " << DebugString(false);

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);
  RecordDownloadCompleted(start_tick_, received_bytes_);

  if (auto_opened_) {
    // If it was already handled by the delegate, do nothing.
  } else if (GetOpenWhenComplete() ||
             ShouldOpenFileBasedOnExtension() ||
             IsTemporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!IsTemporary())
      OpenDownload();

    auto_opened_ = true;
    UpdateObservers();
  }
}

// **** End of Download progression cascade

// An error occurred somewhere.
void DownloadItemImpl::Interrupt(DownloadInterruptReason reason) {
  // Somewhat counter-intuitively, it is possible for us to receive an
  // interrupt after we've already been interrupted.  The generation of
  // interrupts from the file thread Renames and the generation of
  // interrupts from disk writes go through two different mechanisms (driven
  // by rename requests from UI thread and by write requests from IO thread,
  // respectively), and since we choose not to keep state on the File thread,
  // this is the place where the races collide.  It's also possible for
  // interrupts to race with cancels.

  // Whatever happens, the first one to hit the UI thread wins.
  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  last_reason_ = reason;
  TransitionTo(INTERRUPTED_INTERNAL);

  CancelDownloadFile();

  // Cancel the originating URL request.
  request_handle_->CancelRequest();

  RecordDownloadInterrupted(reason, received_bytes_, total_bytes_);
}

void DownloadItemImpl::CancelDownloadFile() {
  // TODO(rdsmith/benjhayden): Remove condition as part of
  // SavePackage integration.
  // download_file_ can be NULL if Interrupt() is called after the download file
  // has been released.
  if (!is_save_package_download_ && download_file_.get()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        // Will be deleted at end of task execution.
        base::Bind(&DownloadFileCancel, base::Passed(&download_file_)));
  }
}

bool DownloadItemImpl::IsDownloadReadyForCompletion(
    const base::Closure& state_change_notification) {
  // If we don't have all the data, the download is not ready for
  // completion.
  if (!AllDataSaved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (GetSafetyState() == DownloadItem::DANGEROUS)
    return false;

  // If the download isn't active (e.g. has been cancelled) it's not
  // ready for completion.
  if (state_ != IN_PROGRESS_INTERNAL)
    return false;

  // If the target filename hasn't been determined, then it's not ready for
  // completion. This is checked in ReadyForDownloadCompletionDone().
  if (GetTargetFilePath().empty())
    return false;

  // This is checked in NeedsRename(). Without this conditional,
  // browser_tests:DownloadTest.DownloadMimeType fails the DCHECK.
  if (target_path_.DirName() != current_path_.DirName())
    return false;

  // Give the delegate a chance to hold up a stop sign.  It'll call
  // use back through the passed callback if it does and that state changes.
  if (!delegate_->ShouldCompleteDownload(this, state_change_notification))
    return false;

  return true;
}

void DownloadItemImpl::TransitionTo(DownloadInternalState new_state) {
  if (state_ == new_state)
    return;

  DownloadInternalState old_state = state_;
  state_ = new_state;

  switch (state_) {
    case COMPLETING_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_COMPLETING,
          base::Bind(&ItemCompletingNetLogCallback, received_bytes_, &hash_));
      break;
    case COMPLETE_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_FINISHED,
          base::Bind(&ItemFinishedNetLogCallback, auto_opened_));
      break;
    case INTERRUPTED_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_INTERRUPTED,
          base::Bind(&ItemInterruptedNetLogCallback, last_reason_,
                     received_bytes_, &hash_state_));
      break;
    case CANCELLED_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_CANCELED,
          base::Bind(&ItemCanceledNetLogCallback, received_bytes_,
                     &hash_state_));
      break;
    default:
      break;
  }

  VLOG(20) << " " << __FUNCTION__ << "()" << " this = " << DebugString(true)
    << " " << InternalToExternalState(old_state)
    << " " << InternalToExternalState(state_);

  // Only update observers on user visible state changes.
  if (InternalToExternalState(old_state) != InternalToExternalState(state_))
    UpdateObservers();

  bool is_done = (state_ != IN_PROGRESS_INTERNAL &&
                  state_ != COMPLETING_INTERNAL);
  bool was_done = (old_state != IN_PROGRESS_INTERNAL &&
                   old_state != COMPLETING_INTERNAL);
  if (is_done && !was_done)
    bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE);
}

void DownloadItemImpl::SetDangerType(DownloadDangerType danger_type) {
  danger_type_ = danger_type;
  // Notify observers if the safety state has changed as a result of the new
  // danger type.
  SafetyState updated_value = IsDangerous() ?
      DownloadItem::DANGEROUS : DownloadItem::SAFE;
  if (updated_value != safety_state_) {
    safety_state_ = updated_value;
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
        base::Bind(&ItemCheckedNetLogCallback, GetDangerType(),
                   GetSafetyState()));
  }
}

void DownloadItemImpl::SetFullPath(const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << "()"
           << " new_path = \"" << new_path.value() << "\""
           << " " << DebugString(true);
  DCHECK(!new_path.empty());

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_RENAMED,
      base::Bind(&ItemRenamedNetLogCallback, &current_path_, &new_path));

  current_path_ = new_path;
  UpdateObservers();
}

// static
DownloadItem::DownloadState DownloadItemImpl::InternalToExternalState(
    DownloadInternalState internal_state) {
  switch (internal_state) {
    case IN_PROGRESS_INTERNAL:
      return IN_PROGRESS;
    case COMPLETING_INTERNAL:
      return IN_PROGRESS;
    case COMPLETE_INTERNAL:
      return COMPLETE;
    case CANCELLED_INTERNAL:
      return CANCELLED;
    case INTERRUPTED_INTERNAL:
      return INTERRUPTED;
    default:
      NOTREACHED();
  }
  return MAX_DOWNLOAD_STATE;
}

// static
DownloadItemImpl::DownloadInternalState
DownloadItemImpl::ExternalToInternalState(
     DownloadState external_state) {
   switch (external_state) {
     case IN_PROGRESS:
       return IN_PROGRESS_INTERNAL;
     case COMPLETE:
       return COMPLETE_INTERNAL;
     case CANCELLED:
       return CANCELLED_INTERNAL;
     case INTERRUPTED:
       return INTERRUPTED_INTERNAL;
     default:
       NOTREACHED();
   }
   return MAX_DOWNLOAD_INTERNAL_STATE;
 }

const char* DownloadItemImpl::DebugDownloadStateString(
    DownloadInternalState state) {
  switch (state) {
    case IN_PROGRESS_INTERNAL:
      return "IN_PROGRESS";
    case COMPLETING_INTERNAL:
      return "COMPLETING";
    case COMPLETE_INTERNAL:
      return "COMPLETE";
    case CANCELLED_INTERNAL:
      return "CANCELLED";
    case INTERRUPTED_INTERNAL:
      return "INTERRUPTED";
    default:
      NOTREACHED() << "Unknown download state " << state;
      return "unknown";
  };
}

}  // namespace content
