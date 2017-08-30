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

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_job_factory.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/download_task_runner.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_parameters_callback.h"
#include "net/log/net_log_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

namespace {

bool DeleteDownloadedFile(const base::FilePath& path) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  // Make sure we only delete files.
  if (base::DirectoryExists(path))
    return true;
  return base::DeleteFile(path, false);
}

void DeleteDownloadedFileDone(
    base::WeakPtr<DownloadItemImpl> item,
    const base::Callback<void(bool)>& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success && item.get())
    item->OnDownloadedFileRemoved();
  callback.Run(success);
}

// Wrapper around DownloadFile::Detach and DownloadFile::Cancel that
// takes ownership of the DownloadFile and hence implicitly destroys it
// at the end of the function.
static base::FilePath DownloadFileDetach(
    std::unique_ptr<DownloadFile> download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath full_path = download_file->FullPath();
  download_file->Detach();
  return full_path;
}

static base::FilePath MakeCopyOfDownloadFile(DownloadFile* download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath temp_file_path;
  if (base::CreateTemporaryFile(&temp_file_path) &&
      base::CopyFile(download_file->FullPath(), temp_file_path)) {
    return temp_file_path;
  } else {
    // Deletes the file at |temp_file_path|.
    if (!base::DirectoryExists(temp_file_path))
      base::DeleteFile(temp_file_path, false);
    temp_file_path.clear();
    return base::FilePath();
  }
}

static void DownloadFileCancel(std::unique_ptr<DownloadFile> download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download_file->Cancel();
}

// Most of the cancellation pathways behave the same whether the cancellation
// was initiated by ther user (CANCELED) or initiated due to browser context
// shutdown (SHUTDOWN).
bool IsCancellation(DownloadInterruptReason reason) {
  return reason == DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN ||
         reason == DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
}

}  // namespace

const uint32_t DownloadItem::kInvalidId = 0;

// The maximum number of attempts we will make to resume automatically.
const int DownloadItemImpl::kMaxAutoResumeAttempts = 5;

DownloadItemImpl::RequestInfo::RequestInfo(
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const std::string& suggested_filename,
    const base::FilePath& forced_file_path,
    ui::PageTransition transition_type,
    bool has_user_gesture,
    const std::string& remote_address,
    base::Time start_time)
    : url_chain(url_chain),
      referrer_url(referrer_url),
      site_url(site_url),
      tab_url(tab_url),
      tab_referrer_url(tab_referrer_url),
      suggested_filename(suggested_filename),
      forced_file_path(forced_file_path),
      transition_type(transition_type),
      has_user_gesture(has_user_gesture),
      remote_address(remote_address),
      start_time(start_time) {}

DownloadItemImpl::RequestInfo::RequestInfo(const GURL& url)
    : url_chain(std::vector<GURL>(1, url)), start_time(base::Time::Now()) {}

DownloadItemImpl::RequestInfo::RequestInfo() = default;

DownloadItemImpl::RequestInfo::RequestInfo(
    const DownloadItemImpl::RequestInfo& other) = default;

DownloadItemImpl::RequestInfo::~RequestInfo() = default;

DownloadItemImpl::DestinationInfo::DestinationInfo(
    const base::FilePath& target_path,
    const base::FilePath& current_path,
    int64_t received_bytes,
    bool all_data_saved,
    const std::string& hash,
    base::Time end_time)
    : target_path(target_path),
      current_path(current_path),
      received_bytes(received_bytes),
      all_data_saved(all_data_saved),
      hash(hash),
      end_time(end_time) {}

DownloadItemImpl::DestinationInfo::DestinationInfo(
    TargetDisposition target_disposition)
    : target_disposition(target_disposition) {}

DownloadItemImpl::DestinationInfo::DestinationInfo() = default;

DownloadItemImpl::DestinationInfo::DestinationInfo(
    const DownloadItemImpl::DestinationInfo& other) = default;

DownloadItemImpl::DestinationInfo::~DestinationInfo() = default;

// Constructor for reading from the history service.
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const std::string& guid,
    uint32_t download_id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_refererr_url,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<DownloadItem::ReceivedSlice>& received_slices,
    const net::NetLogWithSource& net_log)
    : request_info_(url_chain,
                    referrer_url,
                    site_url,
                    tab_url,
                    tab_refererr_url,
                    std::string(),
                    base::FilePath(),
                    ui::PAGE_TRANSITION_LINK,
                    false,
                    std::string(),
                    start_time),
      guid_(guid),
      download_id_(download_id),
      mime_type_(mime_type),
      original_mime_type_(original_mime_type),
      total_bytes_(total_bytes),
      last_reason_(interrupt_reason),
      start_tick_(base::TimeTicks()),
      state_(ExternalToInternalState(state)),
      danger_type_(danger_type),
      delegate_(delegate),
      opened_(opened),
      last_access_time_(last_access_time),
      transient_(transient),
      destination_info_(target_path,
                        current_path,
                        received_bytes,
                        state == COMPLETE,
                        hash,
                        end_time),
      last_modified_time_(last_modified),
      etag_(etag),
      received_slices_(received_slices),
      net_log_(net_log),
      is_updating_observers_(false),
      weak_ptr_factory_(this) {
  delegate_->Attach();
  DCHECK(state_ == COMPLETE_INTERNAL || state_ == INTERRUPTED_INTERNAL ||
         state_ == CANCELLED_INTERNAL);
  DCHECK(base::IsValidGUID(guid_));
  Init(false /* not actively downloading */, SRC_HISTORY_IMPORT);
}

// Constructing for a regular download:
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   uint32_t download_id,
                                   const DownloadCreateInfo& info,
                                   const net::NetLogWithSource& net_log)
    : request_info_(info.url_chain,
                    info.referrer_url,
                    info.site_url,
                    info.tab_url,
                    info.tab_referrer_url,
                    base::UTF16ToUTF8(info.save_info->suggested_name),
                    info.save_info->file_path,
                    info.transition_type ? info.transition_type.value()
                                         : ui::PAGE_TRANSITION_LINK,
                    info.has_user_gesture,
                    info.remote_address,
                    info.start_time),
      guid_(info.guid.empty() ? base::GenerateGUID() : info.guid),
      download_id_(download_id),
      response_headers_(info.response_headers),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      total_bytes_(info.total_bytes),
      last_reason_(info.result),
      start_tick_(base::TimeTicks::Now()),
      state_(INITIAL_INTERNAL),
      delegate_(delegate),
      is_temporary_(!info.transient && !info.save_info->file_path.empty()),
      transient_(info.transient),
      destination_info_(info.save_info->prompt_for_save_location
                            ? TARGET_DISPOSITION_PROMPT
                            : TARGET_DISPOSITION_OVERWRITE),
      last_modified_time_(info.last_modified),
      etag_(info.etag),
      net_log_(net_log),
      is_updating_observers_(false),
      weak_ptr_factory_(this) {
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_ACTIVE_DOWNLOAD);

  // Link the event sources.
  net_log_.AddEvent(
      net::NetLogEventType::DOWNLOAD_URL_REQUEST,
      info.request_net_log.source().ToEventParametersCallback());

  info.request_net_log.AddEvent(
      net::NetLogEventType::DOWNLOAD_STARTED,
      net_log_.source().ToEventParametersCallback());
}

// Constructing for the "Save Page As..." feature:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    uint32_t download_id,
    const base::FilePath& path,
    const GURL& url,
    const std::string& mime_type,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
    const net::NetLogWithSource& net_log)
    : request_info_(url),
      guid_(base::GenerateGUID()),
      download_id_(download_id),
      mime_type_(mime_type),
      original_mime_type_(mime_type),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      delegate_(delegate),
      destination_info_(path, path, 0, false, std::string(), base::Time()),
      net_log_(net_log),
      is_updating_observers_(false),
      weak_ptr_factory_(this) {
  job_ = DownloadJobFactory::CreateJob(this, std::move(request_handle),
                                       DownloadCreateInfo(), true);
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_SAVE_PAGE_AS);
}

DownloadItemImpl::~DownloadItemImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Should always have been nuked before now, at worst in
  // DownloadManager shutdown.
  DCHECK(!download_file_.get());
  CHECK(!is_updating_observers_);

  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);
  delegate_->AssertStateConsistent(this);
  delegate_->Detach();
}

void DownloadItemImpl::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observers_.AddObserver(observer);
}

void DownloadItemImpl::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observers_.RemoveObserver(observer);
}

void DownloadItemImpl::UpdateObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "()";

  // Nested updates should not be allowed.
  DCHECK(!is_updating_observers_);

  is_updating_observers_ = true;
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
  is_updating_observers_ = false;
}

void DownloadItemImpl::ValidateDangerousDownload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsDone());
  DCHECK(IsDangerous());

  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  if (IsDone() || !IsDangerous())
    return;

  RecordDangerousDownloadAccept(GetDangerType(),
                                GetTargetFilePath());

  danger_type_ = DOWNLOAD_DANGER_TYPE_USER_VALIDATED;

  net_log_.AddEvent(
      net::NetLogEventType::DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
      base::Bind(&ItemCheckedNetLogCallback, GetDangerType()));

  UpdateObservers();  // TODO(asanka): This is potentially unsafe. The download
                      // may not be in a consistent state or around at all after
                      // invoking observers. http://crbug.com/586610

  MaybeCompleteDownload();
}

void DownloadItemImpl::StealDangerousDownload(
    bool delete_file_afterward,
    const AcquireFileCallback& callback) {
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsDangerous());
  DCHECK(AllDataSaved());

  if (delete_file_afterward) {
    if (download_file_) {
      base::PostTaskAndReplyWithResult(
          GetDownloadTaskRunner().get(), FROM_HERE,
          base::Bind(&DownloadFileDetach, base::Passed(&download_file_)),
          callback);
    } else {
      callback.Run(GetFullPath());
    }
    destination_info_.current_path.clear();
    Remove();
    // Download item has now been deleted.
  } else if (download_file_) {
    base::PostTaskAndReplyWithResult(
        GetDownloadTaskRunner().get(), FROM_HERE,
        base::Bind(&MakeCopyOfDownloadFile, download_file_.get()), callback);
  } else {
    callback.Run(GetFullPath());
  }
}

void DownloadItemImpl::Pause() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ignore irrelevant states.
  if (IsPaused())
    return;

  switch (state_) {
    case CANCELLED_INTERNAL:
    case COMPLETE_INTERNAL:
    case COMPLETING_INTERNAL:
    case INITIAL_INTERNAL:
    case INTERRUPTED_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case RESUMING_INTERNAL:
      // No active request.
      // TODO(asanka): In the case of RESUMING_INTERNAL, consider setting
      // |DownloadJob::is_paused_| even if there's no request currently
      // associated with this DII. When a request is assigned (due to a
      // resumption, for example) we can honor the |DownloadJob::is_paused_|
      // setting.
      return;

    case IN_PROGRESS_INTERNAL:
    case TARGET_PENDING_INTERNAL:
      job_->Pause();
      UpdateObservers();
      if (download_file_) {
        GetDownloadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&DownloadFile::WasPaused,
                           // Safe because we control download file lifetime.
                           base::Unretained(download_file_.get())));
      }
      return;

    case MAX_DOWNLOAD_INTERNAL_STATE:
    case TARGET_RESOLVED_INTERNAL:
      NOTREACHED();
  }
}

void DownloadItemImpl::Resume() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  switch (state_) {
    case CANCELLED_INTERNAL:  // Nothing to resume.
    case COMPLETE_INTERNAL:
    case COMPLETING_INTERNAL:
    case INITIAL_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case RESUMING_INTERNAL:   // Resumption in progress.
      return;

    case TARGET_PENDING_INTERNAL:
    case IN_PROGRESS_INTERNAL:
      if (!IsPaused())
        return;
      if (job_)
        job_->Resume(true);
      UpdateObservers();
      return;

    case INTERRUPTED_INTERNAL:
      auto_resume_count_ = 0;  // User input resets the counter.
      ResumeInterruptedDownload(ResumptionRequestSource::USER);
      UpdateObservers();
      return;

    case MAX_DOWNLOAD_INTERNAL_STATE:
    case TARGET_RESOLVED_INTERNAL:
      NOTREACHED();
  }
}

void DownloadItemImpl::Cancel(bool user_cancel) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  InterruptAndDiscardPartialState(
      user_cancel ? DOWNLOAD_INTERRUPT_REASON_USER_CANCELED
                  : DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN);
  UpdateObservers();
}

void DownloadItemImpl::Remove() {
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->AssertStateConsistent(this);
  InterruptAndDiscardPartialState(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  UpdateObservers();
  delegate_->AssertStateConsistent(this);

  NotifyRemoved();
  delegate_->DownloadRemoved(this);
  // We have now been deleted.
}

void DownloadItemImpl::OpenDownload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsDone()) {
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
  last_access_time_ = base::Time::Now();
  for (auto& observer : observers_)
    observer.OnDownloadOpened(this);
  delegate_->OpenDownload(this);
}

void DownloadItemImpl::ShowDownloadInShell() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->ShowDownloadInShell(this);
}

uint32_t DownloadItemImpl::GetId() const {
  return download_id_;
}

const std::string& DownloadItemImpl::GetGuid() const {
  return guid_;
}

DownloadItem::DownloadState DownloadItemImpl::GetState() const {
  return InternalToExternalState(state_);
}

DownloadInterruptReason DownloadItemImpl::GetLastReason() const {
  return last_reason_;
}

bool DownloadItemImpl::IsPaused() const {
  return job_ ? job_->is_paused() : false;
}

bool DownloadItemImpl::IsTemporary() const {
  return is_temporary_;
}

bool DownloadItemImpl::CanResume() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (state_) {
    case INITIAL_INTERNAL:
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
    case CANCELLED_INTERNAL:
    case RESUMING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return false;

    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case IN_PROGRESS_INTERNAL:
      return IsPaused();

    case INTERRUPTED_INTERNAL: {
      ResumeMode resume_mode = GetResumeMode();
      // Only allow Resume() calls if the resumption mode requires a user
      // action.
      return resume_mode == RESUME_MODE_USER_RESTART ||
             resume_mode == RESUME_MODE_USER_CONTINUE;
    }

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
  }
  return false;
}

bool DownloadItemImpl::IsDone() const {
  switch (state_) {
    case INITIAL_INTERNAL:
    case COMPLETING_INTERNAL:
    case RESUMING_INTERNAL:
    case TARGET_PENDING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case IN_PROGRESS_INTERNAL:
      return false;

    case COMPLETE_INTERNAL:
    case CANCELLED_INTERNAL:
      return true;

    case INTERRUPTED_INTERNAL:
      return !CanResume();

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
  }
  return false;
}

const GURL& DownloadItemImpl::GetURL() const {
  return request_info_.url_chain.empty() ? GURL::EmptyGURL()
                                         : request_info_.url_chain.back();
}

const std::vector<GURL>& DownloadItemImpl::GetUrlChain() const {
  return request_info_.url_chain;
}

const GURL& DownloadItemImpl::GetOriginalUrl() const {
  // Be careful about taking the front() of possibly-empty vectors!
  // http://crbug.com/190096
  return request_info_.url_chain.empty() ? GURL::EmptyGURL()
                                         : request_info_.url_chain.front();
}

const GURL& DownloadItemImpl::GetReferrerUrl() const {
  return request_info_.referrer_url;
}

const GURL& DownloadItemImpl::GetSiteUrl() const {
  return request_info_.site_url;
}

const GURL& DownloadItemImpl::GetTabUrl() const {
  return request_info_.tab_url;
}

const GURL& DownloadItemImpl::GetTabReferrerUrl() const {
  return request_info_.tab_referrer_url;
}

std::string DownloadItemImpl::GetSuggestedFilename() const {
  return request_info_.suggested_filename;
}

const scoped_refptr<const net::HttpResponseHeaders>&
DownloadItemImpl::GetResponseHeaders() const {
  return response_headers_;
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
  return request_info_.remote_address;
}

bool DownloadItemImpl::HasUserGesture() const {
  return request_info_.has_user_gesture;
}

ui::PageTransition DownloadItemImpl::GetTransitionType() const {
  return request_info_.transition_type;
}

const std::string& DownloadItemImpl::GetLastModifiedTime() const {
  return last_modified_time_;
}

const std::string& DownloadItemImpl::GetETag() const {
  return etag_;
}

bool DownloadItemImpl::IsSavePackageDownload() const {
  return job_ && job_->IsSavePackageDownload();
}

const base::FilePath& DownloadItemImpl::GetFullPath() const {
  return destination_info_.current_path;
}

const base::FilePath& DownloadItemImpl::GetTargetFilePath() const {
  return destination_info_.target_path;
}

const base::FilePath& DownloadItemImpl::GetForcedFilePath() const {
  // TODO(asanka): Get rid of GetForcedFilePath(). We should instead just
  // require that clients respect GetTargetFilePath() if it is already set.
  return request_info_.forced_file_path;
}

base::FilePath DownloadItemImpl::GetFileNameToReportUser() const {
  if (!display_name_.empty())
    return display_name_;
  return GetTargetFilePath().BaseName();
}

DownloadItem::TargetDisposition DownloadItemImpl::GetTargetDisposition() const {
  return destination_info_.target_disposition;
}

const std::string& DownloadItemImpl::GetHash() const {
  return destination_info_.hash;
}

bool DownloadItemImpl::GetFileExternallyRemoved() const {
  return file_externally_removed_;
}

void DownloadItemImpl::DeleteFile(const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetState() != DownloadItem::COMPLETE) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&DeleteDownloadedFileDone,
                       base::WeakPtr<DownloadItemImpl>(), callback, false));
    return;
  }
  if (GetFullPath().empty() || file_externally_removed_) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&DeleteDownloadedFileDone,
                       base::WeakPtr<DownloadItemImpl>(), callback, true));
    return;
  }
  base::PostTaskAndReplyWithResult(
      GetDownloadTaskRunner().get(), FROM_HERE,
      base::Bind(&DeleteDownloadedFile, GetFullPath()),
      base::Bind(&DeleteDownloadedFileDone, weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

bool DownloadItemImpl::IsDangerous() const {
  return (danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
}

DownloadDangerType DownloadItemImpl::GetDangerType() const {
  return danger_type_;
}

bool DownloadItemImpl::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64_t speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining =
      base::TimeDelta::FromSeconds((total_bytes_ - GetReceivedBytes()) / speed);
  return true;
}

int64_t DownloadItemImpl::CurrentSpeed() const {
  if (IsPaused())
    return 0;
  return bytes_per_sec_;
}

int DownloadItemImpl::PercentComplete() const {
  // If the delegate is delaying completion of the download, then we have no
  // idea how long it will take.
  if (delegate_delayed_complete_ || total_bytes_ <= 0)
    return -1;

  return static_cast<int>(GetReceivedBytes() * 100.0 / total_bytes_);
}

bool DownloadItemImpl::AllDataSaved() const {
  return destination_info_.all_data_saved;
}

int64_t DownloadItemImpl::GetTotalBytes() const {
  return total_bytes_;
}

int64_t DownloadItemImpl::GetReceivedBytes() const {
  return destination_info_.received_bytes;
}

const std::vector<DownloadItem::ReceivedSlice>&
DownloadItemImpl::GetReceivedSlices() const {
  return received_slices_;
}

base::Time DownloadItemImpl::GetStartTime() const {
  return request_info_.start_time;
}

base::Time DownloadItemImpl::GetEndTime() const {
  return destination_info_.end_time;
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
  const bool is_complete = GetState() == DownloadItem::COMPLETE;
  return (!IsDone() || is_complete) && !IsTemporary() &&
         !file_externally_removed_;
}

bool DownloadItemImpl::ShouldOpenFileBasedOnExtension() {
  return delegate_->ShouldOpenFileBasedOnExtension(GetTargetFilePath());
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

base::Time DownloadItemImpl::GetLastAccessTime() const {
  return last_access_time_;
}

bool DownloadItemImpl::IsTransient() const {
  return transient_;
}

BrowserContext* DownloadItemImpl::GetBrowserContext() const {
  return delegate_->GetBrowserContext();
}

WebContents* DownloadItemImpl::GetWebContents() const {
  // TODO(rdsmith): Remove null check after removing GetWebContents() from
  // paths that might be used by DownloadItems created from history import.
  // Currently such items have null request_handle_s, where other items
  // (regular and SavePackage downloads) have actual objects off the pointer.
  if (job_)
    return job_->GetWebContents();
  return nullptr;
}

void DownloadItemImpl::OnContentCheckCompleted(DownloadDangerType danger_type,
                                               DownloadInterruptReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(AllDataSaved());

  // Danger type is only allowed to be set on an active download after all data
  // has been saved. This excludes all other states. In particular,
  // OnContentCheckCompleted() isn't allowed on an INTERRUPTED download since
  // such an interruption would need to happen between OnAllDataSaved() and
  // OnContentCheckCompleted() during which no disk or network activity
  // should've taken place.
  DCHECK_EQ(state_, IN_PROGRESS_INTERNAL);
  DVLOG(20) << __func__ << "() danger_type=" << danger_type
            << " download=" << DebugString(true);
  SetDangerType(danger_type);
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    InterruptAndDiscardPartialState(reason);
    DCHECK_EQ(RESUME_MODE_INVALID, GetResumeMode());
  }
  UpdateObservers();
}

void DownloadItemImpl::SetOpenWhenComplete(bool open) {
  open_when_complete_ = open;
}

void DownloadItemImpl::SetOpened(bool opened) {
  opened_ = opened;
}

void DownloadItemImpl::SetLastAccessTime(base::Time last_access_time) {
  last_access_time_ = last_access_time;
  UpdateObservers();
}

void DownloadItemImpl::SetDisplayName(const base::FilePath& name) {
  display_name_ = name;
}

std::string DownloadItemImpl::DebugString(bool verbose) const {
  std::string description =
      base::StringPrintf("{ id = %d"
                         " state = %s",
                         download_id_,
                         DebugDownloadStateString(state_));

  // Construct a string of the URL chain.
  std::string url_list("<none>");
  if (!request_info_.url_chain.empty()) {
    std::vector<GURL>::const_iterator iter = request_info_.url_chain.begin();
    std::vector<GURL>::const_iterator last = request_info_.url_chain.end();
    url_list = (*iter).is_valid() ? (*iter).spec() : "<invalid>";
    ++iter;
    for ( ; verbose && (iter != last); ++iter) {
      url_list += " ->\n\t";
      const GURL& next_url = *iter;
      url_list += next_url.is_valid() ? next_url.spec() : "<invalid>";
    }
  }

  if (verbose) {
    description += base::StringPrintf(
        " total = %" PRId64 " received = %" PRId64
        " reason = %s"
        " paused = %c"
        " resume_mode = %s"
        " auto_resume_count = %d"
        " danger = %d"
        " all_data_saved = %c"
        " last_modified = '%s'"
        " etag = '%s'"
        " has_download_file = %s"
        " url_chain = \n\t\"%s\"\n\t"
        " current_path = \"%" PRFilePath
        "\"\n\t"
        " target_path = \"%" PRFilePath
        "\""
        " referrer = \"%s\""
        " site_url = \"%s\"",
        GetTotalBytes(), GetReceivedBytes(),
        DownloadInterruptReasonToString(last_reason_).c_str(),
        IsPaused() ? 'T' : 'F', DebugResumeModeString(GetResumeMode()),
        auto_resume_count_, GetDangerType(), AllDataSaved() ? 'T' : 'F',
        GetLastModifiedTime().c_str(), GetETag().c_str(),
        download_file_.get() ? "true" : "false", url_list.c_str(),
        GetFullPath().value().c_str(), GetTargetFilePath().value().c_str(),
        GetReferrerUrl().spec().c_str(), GetSiteUrl().spec().c_str());
  } else {
    description += base::StringPrintf(" url = \"%s\"", url_list.c_str());
  }

  description += " }";

  return description;
}

DownloadItemImpl::ResumeMode DownloadItemImpl::GetResumeMode() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only support resumption for HTTP(S).
  if (!GetURL().SchemeIsHTTPOrHTTPS())
    return RESUME_MODE_INVALID;

  // We can't continue without a handle on the intermediate file.
  // We also can't continue if we don't have some verifier to make sure
  // we're getting the same file.
  bool restart_required =
      (GetFullPath().empty() || (etag_.empty() && last_modified_time_.empty()));

  // We won't auto-restart if we've used up our attempts or the
  // download has been paused by user action.
  bool user_action_required =
      (auto_resume_count_ >= kMaxAutoResumeAttempts || IsPaused());

  switch (last_reason_) {
    case DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      break;

    case DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
    // The server disagreed with the file offset that we sent.

    case DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
    // The file on disk was found to not match the expected hash. Discard and
    // start from beginning.

    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
      // The [possibly persisted] file offset disagreed with the file on disk.

      // The intermediate stub is not usable and the server is responding. Hence
      // retrying the request from the beginning is likely to work.
      restart_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
    case DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
    case DOWNLOAD_INTERRUPT_REASON_CRASH:
      // It is not clear whether attempting a resumption is acceptable at this
      // time or whether it would work at all. Hence allow the user to retry the
      // download manually.
      user_action_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
      // There was no space. Require user interaction so that the user may, for
      // example, choose a different location to store the file. Or they may
      // free up some space on the targret device and retry. But try to reuse
      // the partial stub.
      user_action_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      // Assume the partial stub is unusable. Also it may not be possible to
      // restart immediately.
      user_action_required = true;
      restart_required = true;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NONE:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
    case DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
    case DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE:
      return RESUME_MODE_INVALID;
  }

  if (user_action_required && restart_required)
    return RESUME_MODE_USER_RESTART;

  if (restart_required)
    return RESUME_MODE_IMMEDIATE_RESTART;

  if (user_action_required)
    return RESUME_MODE_USER_CONTINUE;

  return RESUME_MODE_IMMEDIATE_CONTINUE;
}

void DownloadItemImpl::UpdateValidatorsOnResumption(
    const DownloadCreateInfo& new_create_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(RESUMING_INTERNAL, state_);
  DCHECK(!new_create_info.url_chain.empty());

  // We are going to tack on any new redirects to our list of redirects.
  // When a download is resumed, the URL used for the resumption request is the
  // one at the end of the previous redirect chain. Tacking additional redirects
  // to the end of this chain ensures that:
  // - If the download needs to be resumed again, the ETag/Last-Modified headers
  //   will be used with the last server that sent them to us.
  // - The redirect chain contains all the servers that were involved in this
  //   download since the initial request, in order.
  std::vector<GURL>::const_iterator chain_iter =
      new_create_info.url_chain.begin();
  if (*chain_iter == request_info_.url_chain.back())
    ++chain_iter;

  // Record some stats. If the precondition failed (the server returned
  // HTTP_PRECONDITION_FAILED), then the download will automatically retried as
  // a full request rather than a partial. Full restarts clobber validators.
  int origin_state = 0;
  bool is_partial = GetReceivedBytes() > 0;
  if (chain_iter != new_create_info.url_chain.end())
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_ADDITIONAL_REDIRECTS;
  if (etag_ != new_create_info.etag ||
      last_modified_time_ != new_create_info.last_modified) {
    received_slices_.clear();
    destination_info_.received_bytes = 0;
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_VALIDATORS_CHANGED;
  }
  if (content_disposition_ != new_create_info.content_disposition)
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_CONTENT_DISPOSITION_CHANGED;
  RecordOriginStateOnResumption(
      is_partial, static_cast<OriginStateOnResumption>(origin_state));

  request_info_.url_chain.insert(request_info_.url_chain.end(), chain_iter,
                                 new_create_info.url_chain.end());
  etag_ = new_create_info.etag;
  last_modified_time_ = new_create_info.last_modified;
  response_headers_ = new_create_info.response_headers;
  content_disposition_ = new_create_info.content_disposition;
  // It is possible that the previous download attempt failed right before the
  // response is received. Need to reset the MIME type.
  mime_type_ = new_create_info.mime_type;

  // Don't update observers. This method is expected to be called just before a
  // DownloadFile is created and Start() is called. The observers will be
  // notified when the download transitions to the IN_PROGRESS state.
}

void DownloadItemImpl::NotifyRemoved() {
  for (auto& observer : observers_)
    observer.OnDownloadRemoved(this);
}

void DownloadItemImpl::OnDownloadedFileRemoved() {
  file_externally_removed_ = true;
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  UpdateObservers();
}

base::WeakPtr<DownloadDestinationObserver>
DownloadItemImpl::DestinationObserverAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const net::NetLogWithSource& DownloadItemImpl::GetNetLogWithSource() const {
  return net_log_;
}

void DownloadItemImpl::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

void DownloadItemImpl::OnAllDataSaved(
    int64_t total_bytes,
    std::unique_ptr<crypto::SecureHash> hash_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!AllDataSaved());
  destination_info_.all_data_saved = true;
  SetTotalBytes(total_bytes);
  UpdateProgress(total_bytes, 0);
  received_slices_.clear();
  SetHashState(std::move(hash_state));
  hash_state_.reset();  // No need to retain hash_state_ since we are done with
                        // the download and don't expect to receive any more
                        // data.

  if (received_bytes_at_length_mismatch_ > 0) {
    if (total_bytes > received_bytes_at_length_mismatch_) {
      RecordDownloadCount(
          MORE_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT);
    } else if (total_bytes == received_bytes_at_length_mismatch_) {
      RecordDownloadCount(
          NO_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT);
    } else {
      // This could happen if the content changes on the server.
    }
  }
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  UpdateObservers();
}

void DownloadItemImpl::MarkAsComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(AllDataSaved());
  destination_info_.end_time = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);
  UpdateObservers();
}

void DownloadItemImpl::DestinationUpdate(
    int64_t bytes_so_far,
    int64_t bytes_per_sec,
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL);

  // There must be no pending deferred_interrupt_reason_.
  DCHECK_EQ(deferred_interrupt_reason_, DOWNLOAD_INTERRUPT_REASON_NONE);

  DVLOG(20) << __func__ << "() so_far=" << bytes_so_far
            << " per_sec=" << bytes_per_sec
            << " download=" << DebugString(true);

  UpdateProgress(bytes_so_far, bytes_per_sec);
  received_slices_ = received_slices;
  if (net_log_.IsCapturing()) {
    net_log_.AddEvent(
        net::NetLogEventType::DOWNLOAD_ITEM_UPDATED,
        net::NetLog::Int64Callback("bytes_so_far", GetReceivedBytes()));
  }

  UpdateObservers();
}

void DownloadItemImpl::DestinationError(
    DownloadInterruptReason reason,
    int64_t bytes_so_far,
    std::unique_ptr<crypto::SecureHash> secure_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL);
  DVLOG(20) << __func__
            << "() reason:" << DownloadInterruptReasonToString(reason)
            << " this:" << DebugString(true);

  InterruptWithPartialState(bytes_so_far, std::move(secure_hash), reason);
  UpdateObservers();
}

void DownloadItemImpl::DestinationCompleted(
    int64_t total_bytes,
    std::unique_ptr<crypto::SecureHash> secure_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  OnAllDataSaved(total_bytes, std::move(secure_hash));
  MaybeCompleteDownload();
}

// **** Download progression cascade

void DownloadItemImpl::Init(bool active,
                            DownloadType download_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string file_name;
  if (download_type == SRC_HISTORY_IMPORT) {
    // target_path_ works for History and Save As versions.
    file_name = GetTargetFilePath().AsUTF8Unsafe();
  } else {
    // See if it's set programmatically.
    file_name = GetForcedFilePath().AsUTF8Unsafe();
    // Possibly has a 'download' attribute for the anchor.
    if (file_name.empty())
      file_name = GetSuggestedFilename();
    // From the URL file name.
    if (file_name.empty())
      file_name = GetURL().ExtractFileName();
  }

  net::NetLogParametersCallback active_data =
      base::Bind(&ItemActivatedNetLogCallback, this, download_type, &file_name);
  if (active) {
    net_log_.BeginEvent(net::NetLogEventType::DOWNLOAD_ITEM_ACTIVE,
                              active_data);
  } else {
    net_log_.AddEvent(net::NetLogEventType::DOWNLOAD_ITEM_ACTIVE,
                            active_data);
  }

  DVLOG(20) << __func__ << "() " << DebugString(true);
}

// We're starting the download.
void DownloadItemImpl::Start(
    std::unique_ptr<DownloadFile> file,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& new_create_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!download_file_.get());
  DVLOG(20) << __func__ << "() this=" << DebugString(true);
  RecordDownloadCount(START_COUNT);

  download_file_ = std::move(file);
  job_ = DownloadJobFactory::CreateJob(this, std::move(req_handle),
                                       new_create_info, false);
  if (job_->IsParallelizable()) {
    RecordParallelizableDownloadCount(START_COUNT, IsParallelDownloadEnabled());
  }

  deferred_interrupt_reason_ = DOWNLOAD_INTERRUPT_REASON_NONE;

  if (state_ == CANCELLED_INTERNAL) {
    // The download was in the process of resuming when it was cancelled. Don't
    // proceed.
    ReleaseDownloadFile(true);
    job_->Cancel(true);
    return;
  }

  // The state could be one of the following:
  //
  // INITIAL_INTERNAL: A normal download attempt.
  //
  // RESUMING_INTERNAL: A resumption attempt. May or may not have been
  //     successful.
  DCHECK(state_ == INITIAL_INTERNAL || state_ == RESUMING_INTERNAL);

  // If the state_ is INITIAL_INTERNAL, then the target path must be empty.
  DCHECK(state_ != INITIAL_INTERNAL || GetTargetFilePath().empty());

  // If a resumption attempted failed, or if the download was DOA, then the
  // download should go back to being interrupted.
  if (new_create_info.result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(!download_file_.get());

    // Download requests that are interrupted by Start() should result in a
    // DownloadCreateInfo with an intact DownloadSaveInfo.
    DCHECK(new_create_info.save_info);

    int64_t offset = new_create_info.save_info->offset;
    std::unique_ptr<crypto::SecureHash> hash_state =
        new_create_info.save_info->hash_state
            ? new_create_info.save_info->hash_state->Clone()
            : nullptr;

    destination_info_.received_bytes = offset;
    hash_state_ = std::move(hash_state);
    destination_info_.hash.clear();
    deferred_interrupt_reason_ = new_create_info.result;
    received_slices_.clear();
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
    DetermineDownloadTarget();
    return;
  }

  if (state_ == INITIAL_INTERNAL) {
    RecordDownloadCount(NEW_DOWNLOAD_COUNT);
    if (job_->IsParallelizable()) {
      RecordParallelizableDownloadCount(NEW_DOWNLOAD_COUNT,
                                        IsParallelDownloadEnabled());
    }
    RecordDownloadMimeType(mime_type_);
    if (!GetBrowserContext()->IsOffTheRecord()) {
      RecordDownloadCount(NEW_DOWNLOAD_COUNT_NORMAL_PROFILE);
      RecordDownloadMimeTypeForNormalProfile(mime_type_);
    }
  }

  // Successful download start.
  DCHECK(download_file_);
  DCHECK(job_);

  if (state_ == RESUMING_INTERNAL)
    UpdateValidatorsOnResumption(new_create_info);

  // If the download is not parallel download during resumption, clear the
  // |received_slices_|.
  if (!job_->IsParallelizable() && !received_slices_.empty()) {
    destination_info_.received_bytes =
        GetMaxContiguousDataBlockSizeFromBeginning(received_slices_);
    received_slices_.clear();
  }

  TransitionTo(TARGET_PENDING_INTERNAL);

  job_->Start(download_file_.get(),
              base::Bind(&DownloadItemImpl::OnDownloadFileInitialized,
                         weak_ptr_factory_.GetWeakPtr()),
              GetReceivedSlices());
}

void DownloadItemImpl::OnDownloadFileInitialized(
    DownloadInterruptReason result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL)
      << "Unexpected state: " << DebugDownloadStateString(state_);

  DVLOG(20) << __func__
            << "() result:" << DownloadInterruptReasonToString(result);
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    ReleaseDownloadFile(true);
    InterruptAndDiscardPartialState(result);
  }

  DetermineDownloadTarget();
}

void DownloadItemImpl::DetermineDownloadTarget() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() " << DebugString(true);

  delegate_->DetermineDownloadTarget(
      this, base::Bind(&DownloadItemImpl::OnDownloadTargetDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
}

// Called by delegate_ when the download target path has been determined.
void DownloadItemImpl::OnDownloadTargetDetermined(
    const base::FilePath& target_path,
    TargetDisposition disposition,
    DownloadDangerType danger_type,
    const base::FilePath& intermediate_path,
    DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);
  DVLOG(20) << __func__ << "() target_path:" << target_path.value()
            << " intermediate_path:" << intermediate_path.value()
            << " disposition:" << disposition << " danger_type:" << danger_type
            << " interrupt_reason:"
            << DownloadInterruptReasonToString(interrupt_reason)
            << " this:" << DebugString(true);

  if (IsCancellation(interrupt_reason) || target_path.empty()) {
    Cancel(true);
    return;
  }

  // There were no other pending errors, and we just failed to determined the
  // download target. The target path, if it is non-empty, should be considered
  // suspect. The safe option here is to interrupt the download without doing an
  // intermediate rename. In the case of a new download, we'll lose the partial
  // data that may have been downloaded, but that should be a small loss.
  if (state_ == TARGET_PENDING_INTERNAL &&
      interrupt_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    deferred_interrupt_reason_ = interrupt_reason;
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
    OnTargetResolved();
    return;
  }

  destination_info_.target_path = target_path;
  destination_info_.target_disposition = disposition;
  SetDangerType(danger_type);

  // This was an interrupted download that was looking for a filename. Resolve
  // early without performing the intermediate rename. If there is a
  // DownloadFile, then that should be renamed to the intermediate name before
  // we can interrupt the download. Otherwise we may lose intermediate state.
  if (state_ == INTERRUPTED_TARGET_PENDING_INTERNAL && !download_file_) {
    OnTargetResolved();
    return;
  }

  // We want the intermediate and target paths to refer to the same directory so
  // that they are both on the same device and subject to same
  // space/permission/availability constraints.
  DCHECK(intermediate_path.DirName() == target_path.DirName());

  // During resumption, we may choose to proceed with the same intermediate
  // file. No rename is necessary if our intermediate file already has the
  // correct name.
  //
  // The intermediate name may change from its original value during filename
  // determination on resumption, for example if the reason for the interruption
  // was the download target running out space, resulting in a user prompt.
  if (intermediate_path == GetFullPath()) {
    OnDownloadRenamedToIntermediateName(DOWNLOAD_INTERRUPT_REASON_NONE,
                                        intermediate_path);
    return;
  }

  // Rename to intermediate name.
  // TODO(asanka): Skip this rename if AllDataSaved() is true. This avoids a
  //               spurious rename when we can just rename to the final
  //               filename. Unnecessary renames may cause bugs like
  //               http://crbug.com/74187.
  DCHECK(!IsSavePackageDownload());
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToIntermediateName,
                 weak_ptr_factory_.GetWeakPtr());
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadFile::RenameAndUniquify,
                     // Safe because we control download file lifetime.
                     base::Unretained(download_file_.get()), intermediate_path,
                     callback));
}

void DownloadItemImpl::OnDownloadRenamedToIntermediateName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);
  DCHECK(download_file_);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  if (DOWNLOAD_INTERRUPT_REASON_NONE == reason) {
    SetFullPath(full_path);
  } else {
    // TODO(asanka): Even though the rename failed, it may still be possible to
    // recover the partial state from the 'before' name.
    deferred_interrupt_reason_ = reason;
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
  }
  OnTargetResolved();
}

void DownloadItemImpl::OnTargetResolved() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  DCHECK((state_ == TARGET_PENDING_INTERNAL &&
          deferred_interrupt_reason_ == DOWNLOAD_INTERRUPT_REASON_NONE) ||
         (state_ == INTERRUPTED_TARGET_PENDING_INTERNAL &&
          deferred_interrupt_reason_ != DOWNLOAD_INTERRUPT_REASON_NONE))
      << " deferred_interrupt_reason_:"
      << DownloadInterruptReasonToString(deferred_interrupt_reason_)
      << " this:" << DebugString(true);

  // This transition is here to ensure that the DownloadItemImpl state machine
  // doesn't transition to INTERRUPTED or IN_PROGRESS from
  // TARGET_PENDING_INTERNAL directly. Doing so without passing through
  // OnTargetResolved() can result in an externally visible state where the
  // download is interrupted but doesn't have a target path associated with it.
  //
  // While not terrible, this complicates the DownloadItem<->Observer
  // relationship since an observer that needs a target path in order to respond
  // properly to an interruption will need to wait for another OnDownloadUpdated
  // notification.  This requirement currently affects all of our UIs.
  TransitionTo(TARGET_RESOLVED_INTERNAL);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != deferred_interrupt_reason_) {
    InterruptWithPartialState(GetReceivedBytes(), std::move(hash_state_),
                              deferred_interrupt_reason_);
    deferred_interrupt_reason_ = DOWNLOAD_INTERRUPT_REASON_NONE;
    UpdateObservers();
    return;
  }

  TransitionTo(IN_PROGRESS_INTERNAL);
  // TODO(asanka): Calling UpdateObservers() prior to MaybeCompleteDownload() is
  // not safe. The download could be in an underminate state after invoking
  // observers. http://crbug.com/586610
  UpdateObservers();
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsSavePackageDownload());

  if (!IsDownloadReadyForCompletion(
          base::Bind(&DownloadItemImpl::MaybeCompleteDownload,
                     weak_ptr_factory_.GetWeakPtr())))
    return;
  // Confirm we're in the proper set of states to be here; have all data, have a
  // history handle, (validated or safe).
  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK(!IsDangerous());
  DCHECK(AllDataSaved());

  OnDownloadCompleting();
}

// Called by MaybeCompleteDownload() when it has determined that the download
// is ready for completion.
void DownloadItemImpl::OnDownloadCompleting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  DVLOG(20) << __func__ << "() " << DebugString(true);
  DCHECK(!GetTargetFilePath().empty());
  DCHECK(!IsDangerous());

  DCHECK(download_file_.get());
  // Unilaterally rename; even if it already has the right name,
  // we need theannotation.
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToFinalName,
                 weak_ptr_factory_.GetWeakPtr());
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadFile::RenameAndAnnotate,
                     base::Unretained(download_file_.get()),
                     GetTargetFilePath(),
                     delegate_->GetApplicationClientIdForFileScanning(),
                     GetURL(), GetReferrerUrl(), callback));
}

void DownloadItemImpl::OnDownloadRenamedToFinalName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsSavePackageDownload());

  // If a cancel or interrupt hit, we'll cancel the DownloadFile, which
  // will result in deleting the file on the file thread.  So we don't
  // care about the name having been changed.
  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  DVLOG(20) << __func__ << "() full_path = \"" << full_path.value() << "\" "
            << DebugString(false);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    // Failure to perform the final rename is considered fatal. TODO(asanka): It
    // may not be, in which case we should figure out whether we can recover the
    // state.
    InterruptAndDiscardPartialState(reason);
    UpdateObservers();
    return;
  }

  DCHECK(GetTargetFilePath() == full_path);

  if (full_path != GetFullPath()) {
    // full_path is now the current and target file path.
    DCHECK(!full_path.empty());
    SetFullPath(full_path);
  }

  // Complete the download and release the DownloadFile.
  DCHECK(download_file_);
  ReleaseDownloadFile(false);

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
    UpdateObservers();
  }
}

void DownloadItemImpl::DelayedDownloadOpened(bool auto_opened) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto_opened_ = auto_opened;
  Completed();
}

void DownloadItemImpl::Completed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(20) << __func__ << "() " << DebugString(false);

  DCHECK(AllDataSaved());
  destination_info_.end_time = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);
  RecordDownloadCompleted(start_tick_, GetReceivedBytes());
  if (!GetBrowserContext()->IsOffTheRecord()) {
    RecordDownloadCount(COMPLETED_COUNT_NORMAL_PROFILE);
  }
  if (job_ && job_->IsParallelizable()) {
    RecordParallelizableDownloadCount(COMPLETED_COUNT,
                                      IsParallelDownloadEnabled());
    int64_t content_length = -1;
    if (response_headers_->response_code() != net::HTTP_PARTIAL_CONTENT) {
      content_length = response_headers_->GetContentLength();
    } else {
      int64_t first_byte = -1;
      int64_t last_byte = -1;
      response_headers_->GetContentRangeFor206(&first_byte, &last_byte,
                                               &content_length);
    }
    if (content_length > 0)
      RecordParallelizableContentLength(content_length);
  }

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
  }
  UpdateObservers();
}

// **** End of Download progression cascade

void DownloadItemImpl::InterruptAndDiscardPartialState(
    DownloadInterruptReason reason) {
  InterruptWithPartialState(0, std::unique_ptr<crypto::SecureHash>(), reason);
}

void DownloadItemImpl::InterruptWithPartialState(
    int64_t bytes_so_far,
    std::unique_ptr<crypto::SecureHash> hash_state,
    DownloadInterruptReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, reason);
  DVLOG(20) << __func__
            << "() reason:" << DownloadInterruptReasonToString(reason)
            << " bytes_so_far:" << bytes_so_far
            << " hash_state:" << (hash_state ? "Valid" : "Invalid")
            << " this=" << DebugString(true);

  // Somewhat counter-intuitively, it is possible for us to receive an
  // interrupt after we've already been interrupted.  The generation of
  // interrupts from the file thread Renames and the generation of
  // interrupts from disk writes go through two different mechanisms (driven
  // by rename requests from UI thread and by write requests from IO thread,
  // respectively), and since we choose not to keep state on the File thread,
  // this is the place where the races collide.  It's also possible for
  // interrupts to race with cancels.
  switch (state_) {
    case CANCELLED_INTERNAL:
    // If the download is already cancelled, then there's no point in
    // transitioning out to interrupted.
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
      // Already complete.
      return;

    case INITIAL_INTERNAL:
    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
      return;

    case TARGET_PENDING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      // Postpone recognition of this error until after file name determination
      // has completed and the intermediate file has been renamed to simplify
      // resumption conditions. The target determination logic is much simpler
      // if the state of the download remains constant until that stage
      // completes.
      //
      // current_path_ may be empty because it is possible for DownloadItem to
      // receive a DestinationError prior to the download file initialization
      // complete callback.
      if (!IsCancellation(reason)) {
        UpdateProgress(bytes_so_far, 0);
        SetHashState(std::move(hash_state));
        deferred_interrupt_reason_ = reason;
        TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
        return;
      }
    // else - Fallthrough for cancellation handling which is equivalent to the
    // IN_PROGRESS state.

    case IN_PROGRESS_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
      // last_reason_ needs to be set for GetResumeMode() to work.
      last_reason_ = reason;

      if (download_file_) {
        ResumeMode resume_mode = GetResumeMode();
        ReleaseDownloadFile(resume_mode != RESUME_MODE_IMMEDIATE_CONTINUE &&
                            resume_mode != RESUME_MODE_USER_CONTINUE);
      }
      break;

    case RESUMING_INTERNAL:
    case INTERRUPTED_INTERNAL:
      DCHECK(!download_file_);
      // The first non-cancel interrupt reason wins in cases where multiple
      // things go wrong.
      if (!IsCancellation(reason))
        return;

      last_reason_ = reason;
      if (!GetFullPath().empty()) {
        // There is no download file and this is transitioning from INTERRUPTED
        // to CANCELLED. The intermediate file is no longer usable, and should
        // be deleted.
        GetDownloadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(base::IgnoreResult(&DeleteDownloadedFile),
                                      GetFullPath()));
        destination_info_.current_path.clear();
      }
      break;
  }

  // Reset all data saved, as even if we did save all the data we're going to go
  // through another round of downloading when we resume. There's a potential
  // problem here in the abstract, as if we did download all the data and then
  // run into a continuable error, on resumption we won't download any more
  // data.  However, a) there are currently no continuable errors that can occur
  // after we download all the data, and b) if there were, that would probably
  // simply result in a null range request, which would generate a
  // DestinationCompleted() notification from the DownloadFile, which would
  // behave properly with setting all_data_saved_ to false here.
  destination_info_.all_data_saved = false;

  if (GetFullPath().empty()) {
    hash_state_.reset();
    destination_info_.hash.clear();
    destination_info_.received_bytes = 0;
    received_slices_.clear();
  } else {
    UpdateProgress(bytes_so_far, 0);
    SetHashState(std::move(hash_state));
  }

  if (job_)
    job_->Cancel(false);

  if (IsCancellation(reason)) {
    if (IsDangerous()) {
      RecordDangerousDownloadDiscard(
          reason == DOWNLOAD_INTERRUPT_REASON_USER_CANCELED
              ? DOWNLOAD_DISCARD_DUE_TO_USER_ACTION
              : DOWNLOAD_DISCARD_DUE_TO_SHUTDOWN,
          GetDangerType(), GetTargetFilePath());
    }

    RecordDownloadCount(CANCELLED_COUNT);
    if (job_ && job_->IsParallelizable()) {
      RecordParallelizableDownloadCount(CANCELLED_COUNT,
                                        IsParallelDownloadEnabled());
    }
    DCHECK_EQ(last_reason_, reason);
    TransitionTo(CANCELLED_INTERNAL);
    return;
  }

  RecordDownloadInterrupted(reason, GetReceivedBytes(), total_bytes_,
                            job_ && job_->IsParallelizable(),
                            IsParallelDownloadEnabled());
  if (reason == DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH)
    received_bytes_at_length_mismatch_ = GetReceivedBytes();

  if (!GetWebContents())
    RecordDownloadCount(INTERRUPTED_WITHOUT_WEBCONTENTS);

  // TODO(asanka): This is not good. We can transition to interrupted from
  // target-pending, which is something we don't want to do. Perhaps we should
  // explicitly transition to target-resolved prior to switching to interrupted.
  DCHECK_EQ(last_reason_, reason);
  TransitionTo(INTERRUPTED_INTERNAL);
  AutoResumeIfValid();
}

void DownloadItemImpl::UpdateProgress(
    int64_t bytes_so_far,
    int64_t bytes_per_sec) {
  destination_info_.received_bytes = bytes_so_far;
  bytes_per_sec_ = bytes_per_sec;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (bytes_so_far > total_bytes_)
    total_bytes_ = 0;
}

void DownloadItemImpl::SetHashState(
    std::unique_ptr<crypto::SecureHash> hash_state) {
  hash_state_ = std::move(hash_state);
  if (!hash_state_) {
    destination_info_.hash.clear();
    return;
  }

  std::unique_ptr<crypto::SecureHash> clone_of_hash_state(hash_state_->Clone());
  std::vector<char> hash_value(clone_of_hash_state->GetHashLength());
  clone_of_hash_state->Finish(&hash_value.front(), hash_value.size());
  destination_info_.hash.assign(hash_value.begin(), hash_value.end());
}

void DownloadItemImpl::ReleaseDownloadFile(bool destroy_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() destroy_file:" << destroy_file;

  if (destroy_file) {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        // Will be deleted at end of task execution.
        base::BindOnce(&DownloadFileCancel, base::Passed(&download_file_)));
    // Avoid attempting to reuse the intermediate file by clearing out
    // current_path_ and received slices.
    destination_info_.current_path.clear();
    received_slices_.clear();
  } else {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&DownloadFileDetach),
                                  // Will be deleted at end of task execution.
                                  base::Passed(&download_file_)));
  }
  // Don't accept any more messages from the DownloadFile, and null
  // out any previous "all data received".  This also breaks links to
  // other entities we've given out weak pointers to.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool DownloadItemImpl::IsDownloadReadyForCompletion(
    const base::Closure& state_change_notification) {
  // If the download hasn't progressed to the IN_PROGRESS state, then it's not
  // ready for completion.
  if (state_ != IN_PROGRESS_INTERNAL)
    return false;

  // If we don't have all the data, the download is not ready for
  // completion.
  if (!AllDataSaved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (IsDangerous())
    return false;

  // Check for consistency before invoking delegate. Since there are no pending
  // target determination calls and the download is in progress, both the target
  // and current paths should be non-empty and they should point to the same
  // directory.
  DCHECK(!GetTargetFilePath().empty());
  DCHECK(!GetFullPath().empty());
  DCHECK(GetTargetFilePath().DirName() == GetFullPath().DirName());

  // Give the delegate a chance to hold up a stop sign.  It'll call
  // use back through the passed callback if it does and that state changes.
  if (!delegate_->ShouldCompleteDownload(this, state_change_notification))
    return false;

  return true;
}

void DownloadItemImpl::TransitionTo(DownloadInternalState new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ == new_state)
    return;

  DownloadInternalState old_state = state_;
  state_ = new_state;

  DCHECK(IsSavePackageDownload()
             ? IsValidSavePackageStateTransition(old_state, new_state)
             : IsValidStateTransition(old_state, new_state))
      << "Invalid state transition from:" << DebugDownloadStateString(old_state)
      << " to:" << DebugDownloadStateString(new_state);

  switch (state_) {
    case INITIAL_INTERNAL:
      NOTREACHED();
      break;

    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
      break;

    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, deferred_interrupt_reason_)
          << "Interrupt reason must be set prior to transitioning into "
             "TARGET_PENDING";
      break;

    case IN_PROGRESS_INTERNAL:
      DCHECK(!GetFullPath().empty()) << "Current output path must be known.";
      DCHECK(!GetTargetFilePath().empty()) << "Target path must be known.";
      DCHECK(GetFullPath().DirName() == GetTargetFilePath().DirName())
          << "Current output directory must match target directory.";
      DCHECK(download_file_) << "Output file must be owned by download item.";
      DCHECK(job_) << "Must have active download job.";
      DCHECK(!job_->is_paused())
          << "At the time a download enters IN_PROGRESS state, "
             "it must not be paused.";
      break;

    case COMPLETING_INTERNAL:
      DCHECK(AllDataSaved()) << "All data must be saved prior to completion.";
      DCHECK(!download_file_)
          << "Download file must be released prior to completion.";
      DCHECK(!GetTargetFilePath().empty()) << "Target path must be known.";
      DCHECK(GetFullPath() == GetTargetFilePath())
          << "Current output path must match target path.";

      net_log_.AddEvent(
          net::NetLogEventType::DOWNLOAD_ITEM_COMPLETING,
          base::Bind(&ItemCompletingNetLogCallback, GetReceivedBytes(),
                     &destination_info_.hash));
      break;

    case COMPLETE_INTERNAL:
      net_log_.AddEvent(
          net::NetLogEventType::DOWNLOAD_ITEM_FINISHED,
          base::Bind(&ItemFinishedNetLogCallback, auto_opened_));
      break;

    case INTERRUPTED_INTERNAL:
      DCHECK(!download_file_)
          << "Download file must be released prior to interruption.";
      DCHECK_NE(last_reason_, DOWNLOAD_INTERRUPT_REASON_NONE);
      net_log_.AddEvent(net::NetLogEventType::DOWNLOAD_ITEM_INTERRUPTED,
                        base::Bind(&ItemInterruptedNetLogCallback, last_reason_,
                                   GetReceivedBytes()));
      break;

    case RESUMING_INTERNAL:
      net_log_.AddEvent(net::NetLogEventType::DOWNLOAD_ITEM_RESUMED,
                        base::Bind(&ItemResumingNetLogCallback, false,
                                   last_reason_, GetReceivedBytes()));
      break;

    case CANCELLED_INTERNAL:
      net_log_.AddEvent(
          net::NetLogEventType::DOWNLOAD_ITEM_CANCELED,
          base::Bind(&ItemCanceledNetLogCallback, GetReceivedBytes()));
      break;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
      break;
  }

  DVLOG(20) << __func__ << "() from:" << DebugDownloadStateString(old_state)
            << " to:" << DebugDownloadStateString(state_)
            << " this = " << DebugString(true);
  bool is_done =
      (state_ == COMPLETE_INTERNAL || state_ == INTERRUPTED_INTERNAL ||
       state_ == RESUMING_INTERNAL || state_ == CANCELLED_INTERNAL);
  bool was_done =
      (old_state == COMPLETE_INTERNAL || old_state == INTERRUPTED_INTERNAL ||
       old_state == RESUMING_INTERNAL || old_state == CANCELLED_INTERNAL);

  // Termination
  if (is_done && !was_done)
    net_log_.EndEvent(net::NetLogEventType::DOWNLOAD_ITEM_ACTIVE);

  // Resumption
  if (was_done && !is_done) {
    std::string file_name(GetTargetFilePath().BaseName().AsUTF8Unsafe());
    net_log_.BeginEvent(net::NetLogEventType::DOWNLOAD_ITEM_ACTIVE,
                              base::Bind(&ItemActivatedNetLogCallback, this,
                                         SRC_ACTIVE_DOWNLOAD, &file_name));
  }
}

void DownloadItemImpl::SetDangerType(DownloadDangerType danger_type) {
  if (danger_type != danger_type_) {
    net_log_.AddEvent(
        net::NetLogEventType::DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
        base::Bind(&ItemCheckedNetLogCallback, danger_type));
  }
  // Only record the Malicious UMA stat if it's going from {not malicious} ->
  // {malicious}.
  if ((danger_type_ == DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT) &&
      (danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
       danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
       danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
       danger_type == DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED)) {
    RecordMaliciousDownloadClassified(danger_type);
  }
  danger_type_ = danger_type;
}

void DownloadItemImpl::SetFullPath(const base::FilePath& new_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << __func__ << "() new_path = \"" << new_path.value() << "\" "
            << DebugString(true);
  DCHECK(!new_path.empty());

  net_log_.AddEvent(net::NetLogEventType::DOWNLOAD_ITEM_RENAMED,
                    base::Bind(&ItemRenamedNetLogCallback,
                               &destination_info_.current_path, &new_path));

  destination_info_.current_path = new_path;
}

void DownloadItemImpl::AutoResumeIfValid() {
  DVLOG(20) << __func__ << "() " << DebugString(true);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ResumeMode mode = GetResumeMode();

  if (mode != RESUME_MODE_IMMEDIATE_RESTART &&
      mode != RESUME_MODE_IMMEDIATE_CONTINUE) {
    return;
  }

  auto_resume_count_++;

  ResumeInterruptedDownload(ResumptionRequestSource::AUTOMATIC);
}

void DownloadItemImpl::ResumeInterruptedDownload(
    ResumptionRequestSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If we're not interrupted, ignore the request; our caller is drunk.
  if (state_ != INTERRUPTED_INTERNAL)
    return;

  // We are starting a new request. Shake off all pending operations.
  DCHECK(!download_file_);
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset the appropriate state if restarting.
  ResumeMode mode = GetResumeMode();
  if (mode == RESUME_MODE_IMMEDIATE_RESTART ||
      mode == RESUME_MODE_USER_RESTART) {
    DCHECK(GetFullPath().empty());
    destination_info_.received_bytes = 0;
    last_modified_time_.clear();
    etag_.clear();
    destination_info_.hash.clear();
    hash_state_.reset();
    received_slices_.clear();
  }

  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartitionForSite(GetBrowserContext(),
                                                 request_info_.site_url);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_manager_resume", R"(
        semantics {
          sender: "Download Manager"
          description:
            "When user resumes downloading a file, a network request is made "
            "to fetch it."
          trigger:
            "User resumes a download."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but it is activated "
            "by direct user action."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  // Avoid using the WebContents even if it's still around. Resumption requests
  // are consistently routed through the no-renderer code paths so that the
  // request will not be dropped if the WebContents (and by extension, the
  // associated renderer) goes away before a response is received.
  std::unique_ptr<DownloadUrlParameters> download_params(
      new DownloadUrlParameters(GetURL(),
                                storage_partition->GetURLRequestContext(),
                                traffic_annotation));
  download_params->set_file_path(GetFullPath());
  if (received_slices_.size() > 0) {
    std::vector<DownloadItem::ReceivedSlice> slices_to_download
        = FindSlicesToDownload(received_slices_);
    download_params->set_offset(slices_to_download[0].offset);
  } else {
    download_params->set_offset(GetReceivedBytes());
  }
  download_params->set_last_modified(GetLastModifiedTime());
  download_params->set_etag(GetETag());
  download_params->set_hash_of_partial_file(GetHash());
  download_params->set_hash_state(std::move(hash_state_));

  // Note that resumed downloads disallow redirects. Hence the referrer URL
  // (which is the contents of the Referer header for the last download request)
  // will only be sent to the URL returned by GetURL().
  download_params->set_referrer(
      Referrer(GetReferrerUrl(), blink::kWebReferrerPolicyAlways));

  TransitionTo(RESUMING_INTERNAL);
  RecordDownloadSource(source == ResumptionRequestSource::USER
                           ? INITIATED_BY_MANUAL_RESUMPTION
                           : INITIATED_BY_AUTOMATIC_RESUMPTION);
  delegate_->ResumeInterruptedDownload(std::move(download_params), GetId());

  if (job_)
    job_->Resume(false);
}

// static
DownloadItem::DownloadState DownloadItemImpl::InternalToExternalState(
    DownloadInternalState internal_state) {
  switch (internal_state) {
    case INITIAL_INTERNAL:
    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    // TODO(asanka): Introduce an externally visible state to distinguish
    // between the above states and IN_PROGRESS_INTERNAL. The latter (the
    // state where the download is active and has a known target) is the state
    // that most external users are interested in.
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
    case RESUMING_INTERNAL:
      return IN_PROGRESS;
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED();
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

// static
bool DownloadItemImpl::IsValidSavePackageStateTransition(
    DownloadInternalState from,
    DownloadInternalState to) {
#if DCHECK_IS_ON()
  switch (from) {
    case INITIAL_INTERNAL:
    case TARGET_PENDING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
    case INTERRUPTED_INTERNAL:
    case RESUMING_INTERNAL:
    case CANCELLED_INTERNAL:
      return false;

    case IN_PROGRESS_INTERNAL:
      return to == CANCELLED_INTERNAL || to == COMPLETE_INTERNAL;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
  }
  return false;
#else
  return true;
#endif
}

// static
bool DownloadItemImpl::IsValidStateTransition(DownloadInternalState from,
                                              DownloadInternalState to) {
#if DCHECK_IS_ON()
  switch (from) {
    case INITIAL_INTERNAL:
      return to == TARGET_PENDING_INTERNAL ||
             to == INTERRUPTED_TARGET_PENDING_INTERNAL;

    case TARGET_PENDING_INTERNAL:
      return to == INTERRUPTED_TARGET_PENDING_INTERNAL ||
             to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case TARGET_RESOLVED_INTERNAL:
      return to == IN_PROGRESS_INTERNAL || to == INTERRUPTED_INTERNAL ||
             to == CANCELLED_INTERNAL;

    case IN_PROGRESS_INTERNAL:
      return to == COMPLETING_INTERNAL || to == CANCELLED_INTERNAL ||
             to == INTERRUPTED_INTERNAL;

    case COMPLETING_INTERNAL:
      return to == COMPLETE_INTERNAL;

    case COMPLETE_INTERNAL:
      return false;

    case INTERRUPTED_INTERNAL:
      return to == RESUMING_INTERNAL || to == CANCELLED_INTERNAL;

    case RESUMING_INTERNAL:
      return to == TARGET_PENDING_INTERNAL ||
             to == INTERRUPTED_TARGET_PENDING_INTERNAL ||
             to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case CANCELLED_INTERNAL:
      return false;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
  }
  return false;
#else
  return true;
#endif  // DCHECK_IS_ON()
}

const char* DownloadItemImpl::DebugDownloadStateString(
    DownloadInternalState state) {
  switch (state) {
    case INITIAL_INTERNAL:
      return "INITIAL";
    case TARGET_PENDING_INTERNAL:
      return "TARGET_PENDING";
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return "INTERRUPTED_TARGET_PENDING";
    case TARGET_RESOLVED_INTERNAL:
      return "TARGET_RESOLVED";
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
    case RESUMING_INTERNAL:
      return "RESUMING";
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED() << "Unknown download state " << state;
  return "unknown";
}

const char* DownloadItemImpl::DebugResumeModeString(ResumeMode mode) {
  switch (mode) {
    case RESUME_MODE_INVALID:
      return "INVALID";
    case RESUME_MODE_IMMEDIATE_CONTINUE:
      return "IMMEDIATE_CONTINUE";
    case RESUME_MODE_IMMEDIATE_RESTART:
      return "IMMEDIATE_RESTART";
    case RESUME_MODE_USER_CONTINUE:
      return "USER_CONTINUE";
    case RESUME_MODE_USER_RESTART:
      return "USER_RESTART";
  }
  NOTREACHED() << "Unknown resume mode " << mode;
  return "unknown";
}

}  // namespace content
