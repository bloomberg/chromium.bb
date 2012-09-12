// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File method ordering: Methods in this file are in the same order
// as in download_item_impl.h, with the following exception: The public
// interfaces DelayedDownloadOpened, OnDownloadTargetDetermined, and
// OnDownloadCompleting are placed in chronological order with the other
// (private) routines that together define a DownloadItem's state transitions
// as the download progresses.  See "Download progression cascade" later in
// this file.

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
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::DownloadFile;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadManager;
using content::DownloadPersistentStoreInfo;
using content::WebContents;

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

}  // namespace

namespace content {

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressively getting
// more negative.
// static
const int DownloadItem::kUninitializedHandle = 0;

const char DownloadItem::kEmptyFileHash[] = "";

}

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressively getting
// more negative.

// Constructor for reading from the history service.
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   DownloadId download_id,
                                   const DownloadPersistentStoreInfo& info,
                                   const net::BoundNetLog& bound_net_log)
    : download_id_(download_id),
      current_path_(info.path),
      target_path_(info.path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(1, info.url),
      referrer_url_(info.referrer_url),
      transition_type_(content::PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      total_bytes_(info.total_bytes),
      received_bytes_(info.received_bytes),
      bytes_per_sec_(0),
      last_reason_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks()),
      state_(info.state),
      danger_type_(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(info.start_time),
      end_time_(info.end_time),
      db_handle_(info.db_handle),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_persisted_(true),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(info.opened),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  if (IsInProgress())
    state_ = CANCELLED;
  if (IsComplete())
    all_data_saved_ = true;
  Init(false /* not actively downloading */,
       download_net_logs::SRC_HISTORY_IMPORT);
}

// Constructing for a regular download:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const DownloadCreateInfo& info,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const net::BoundNetLog& bound_net_log)
    : request_handle_(request_handle.Pass()),
      download_id_(info.download_id),
      target_disposition_(
          (info.prompt_user_for_save_location) ?
              TARGET_DISPOSITION_PROMPT : TARGET_DISPOSITION_OVERWRITE),
      url_chain_(info.url_chain),
      referrer_url_(info.referrer_url),
      suggested_filename_(UTF16ToUTF8(info.save_info.suggested_name)),
      forced_file_path_(info.save_info.file_path),
      transition_type_(info.transition_type),
      has_user_gesture_(info.has_user_gesture),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      referrer_charset_(info.referrer_charset),
      remote_address_(info.remote_address),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_reason_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      danger_type_(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(info.start_time),
      db_handle_(DownloadItem::kUninitializedHandle),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_persisted_(false),
      is_temporary_(!info.save_info.file_path.empty()),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  Init(true /* actively downloading */,
       download_net_logs::SRC_NEW_DOWNLOAD);

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
    : request_handle_(new NullDownloadRequestHandle()),
      download_id_(download_id),
      current_path_(path),
      target_path_(path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(1, url),
      referrer_url_(GURL()),
      transition_type_(content::PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      mime_type_(mime_type),
      original_mime_type_(mime_type),
      total_bytes_(0),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_reason_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      danger_type_(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(base::Time::Now()),
      db_handle_(DownloadItem::kUninitializedHandle),
      delegate_(delegate),
      is_paused_(false),
      open_when_complete_(false),
      file_externally_removed_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      is_persisted_(false),
      is_temporary_(false),
      all_data_saved_(false),
      opened_(false),
      open_enabled_(true),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  delegate_->Attach();
  Init(true /* actively downloading */,
       download_net_logs::SRC_SAVE_PAGE_AS);
}

DownloadItemImpl::~DownloadItemImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK_EQ(DANGEROUS, GetSafetyState());

  UMA_HISTOGRAM_ENUMERATION("Download.DangerousDownloadValidated",
                            GetDangerType(),
                            content::DOWNLOAD_DANGER_TYPE_MAX);

  safety_state_ = DANGEROUS_BUT_VALIDATED;

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
      base::Bind(&download_net_logs::ItemCheckedCallback,
                 GetDangerType(), GetSafetyState()));

  UpdateObservers();

  delegate_->MaybeCompleteDownload(this);
}

void DownloadItemImpl::TogglePause() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(IsInProgress());
  if (is_paused_)
    request_handle_->ResumeRequest();
  else
    request_handle_->PauseRequest();
  is_paused_ = !is_paused_;
  UpdateObservers();
}

void DownloadItemImpl::Cancel(bool user_cancel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  last_reason_ = user_cancel ?
      content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED :
      content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  if (!IsPartialDownload()) {
    // Small downloads might be complete before this method has
    // a chance to run.
    return;
  }

  download_stats::RecordDownloadCount(download_stats::CANCELLED_COUNT);

  TransitionTo(CANCELLED);
  if (user_cancel)
    delegate_->DownloadStopped(this);
}

void DownloadItemImpl::Delete(DeleteReason reason) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (reason) {
    case DELETE_DUE_TO_USER_DISCARD:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.UserDiscard", GetDangerType(),
          content::DOWNLOAD_DANGER_TYPE_MAX);
      break;
    case DELETE_DUE_TO_BROWSER_SHUTDOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.Discard", GetDangerType(),
          content::DOWNLOAD_DANGER_TYPE_MAX);
      break;
    default:
      NOTREACHED();
  }

  // TODO(asanka): Avoid deleting a file that is still owned by DownloadFile.
  if (!current_path_.empty())
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&DeleteDownloadedFile, current_path_));
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

  if (IsPartialDownload()) {
    // We don't honor the open_when_complete_ flag for temporary
    // downloads. Don't set it because it shows up in the UI.
    if (!IsTemporary())
      open_when_complete_ = !open_when_complete_;
    return;
  }

  if (!IsComplete() || file_externally_removed_)
    return;

  // Ideally, we want to detect errors in opening and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the open.
  delegate_->CheckForFileRemoval(this);
  download_stats::RecordOpen(GetEndTime(), !GetOpened());
  opened_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
  delegate_->DownloadOpened(this);

  // For testing: If download opening is disabled on this item,
  // make the rest of the routine a no-op.
  if (!open_enabled_)
    return;

  content::GetContentClient()->browser()->OpenItem(GetFullPath());
}

void DownloadItemImpl::ShowDownloadInShell() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::GetContentClient()->browser()->ShowItemInFolder(GetFullPath());
}

int32 DownloadItemImpl::GetId() const {
  return download_id_.local();
}

DownloadId DownloadItemImpl::GetGlobalId() const {
  return download_id_;
}

int64 DownloadItemImpl::GetDbHandle() const {
  return db_handle_;
}

DownloadItem::DownloadState DownloadItemImpl::GetState() const {
  return state_;
}

content::DownloadInterruptReason DownloadItemImpl::GetLastReason() const {
  return last_reason_;
}

bool DownloadItemImpl::IsPaused() const {
  return is_paused_;
}

bool DownloadItemImpl::IsTemporary() const {
  return is_temporary_;
}

bool DownloadItemImpl::IsPersisted() const {
  return is_persisted_;
}

// TODO(ahendrickson) -- Move |INTERRUPTED| from |IsCancelled()| to
// |IsPartialDownload()|, when resuming interrupted downloads is implemented.
bool DownloadItemImpl::IsPartialDownload() const {
  return (state_ == IN_PROGRESS);
}

bool DownloadItemImpl::IsInProgress() const {
  return (state_ == IN_PROGRESS);
}

bool DownloadItemImpl::IsCancelled() const {
  return (state_ == CANCELLED) ||
         (state_ == INTERRUPTED);
}

bool DownloadItemImpl::IsInterrupted() const {
  return (state_ == INTERRUPTED);
}

bool DownloadItemImpl::IsComplete() const {
  return (state_ == COMPLETE);
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

std::string DownloadItemImpl::GetReferrerCharset() const {
  return referrer_charset_;
}

std::string DownloadItemImpl::GetRemoteAddress() const {
  return remote_address_;
}

bool DownloadItemImpl::HasUserGesture() const {
  return has_user_gesture_;
};

content::PageTransition DownloadItemImpl::GetTransitionType() const {
  return transition_type_;
};

const std::string& DownloadItemImpl::GetLastModifiedTime() const {
  return last_modified_time_;
}

const std::string& DownloadItemImpl::GetETag() const {
  return etag_;
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
  return (danger_type_ == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type_ == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type_ == content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
#else
  return (danger_type_ == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
#endif
}

content::DownloadDangerType DownloadItemImpl::GetDangerType() const {
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
  return !IsCancelled() && !file_externally_removed_;
}

bool DownloadItemImpl::CanOpenDownload() {
  return !file_externally_removed_;
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

bool DownloadItemImpl::MatchesQuery(const string16& query) const {
  if (query.empty())
    return true;

  DCHECK_EQ(query, base::i18n::ToLower(query));

  string16 url_raw(UTF8ToUTF16(GetURL().spec()));
  if (base::i18n::StringSearchIgnoringCaseAndAccents(
          query, url_raw, NULL, NULL)) {
    return true;
  }

  // TODO(phajdan.jr): write a test case for the following code.
  // A good test case would be:
  //   "/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd",
  //   L"/\x4f60\x597d\x4f60\x597d",
  //   "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD"
  std::string languages;
  languages = content::GetContentClient()->browser()->GetAcceptLangs(
      GetBrowserContext());
  string16 url_formatted(net::FormatUrl(GetURL(), languages));
  if (base::i18n::StringSearchIgnoringCaseAndAccents(
          query, url_formatted, NULL, NULL)) {
    return true;
  }

  // TODO(asanka): Change this to GetTargetFilePath() once DownloadQuery has
  //               been modified to work with target paths.
  string16 path(GetFullPath().LossyDisplayName());
  return base::i18n::StringSearchIgnoringCaseAndAccents(
      query, path, NULL, NULL);
}

DownloadPersistentStoreInfo DownloadItemImpl::GetPersistentStoreInfo() const {
  // TODO(asanka): Persist GetTargetFilePath() as well.
  return DownloadPersistentStoreInfo(GetFullPath(),
                                     GetURL(),
                                     GetReferrerUrl(),
                                     GetStartTime(),
                                     GetEndTime(),
                                     GetReceivedBytes(),
                                     GetTotalBytes(),
                                     GetState(),
                                     GetDbHandle(),
                                     GetOpened());
}

content::BrowserContext* DownloadItemImpl::GetBrowserContext() const {
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

void DownloadItemImpl::OnContentCheckCompleted(
    content::DownloadDangerType danger_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(AllDataSaved());
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
                         DebugDownloadStateString(GetState()));

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
        " total = %" PRId64
        " received = %" PRId64
        " reason = %s"
        " paused = %c"
        " safety = %s"
        " last_modified = '%s'"
        " etag = '%s'"
        " url_chain = \n\t\"%s\"\n\t"
        " full_path = \"%" PRFilePath "\""
        " target_path = \"%" PRFilePath "\"",
        GetDbHandle(),
        GetTotalBytes(),
        GetReceivedBytes(),
        InterruptReasonDebugString(last_reason_).c_str(),
        IsPaused() ? 'T' : 'F',
        DebugSafetyStateString(GetSafetyState()),
        GetLastModifiedTime().c_str(),
        GetETag().c_str(),
        url_list.c_str(),
        GetFullPath().value().c_str(),
        GetTargetFilePath().value().c_str());
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
  UpdateObservers();
}

void DownloadItemImpl::OffThreadCancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  request_handle_->CancelRequest();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::CancelDownload,
                 delegate_->GetDownloadFileManager(), download_id_));
}

// An error occurred somewhere.
void DownloadItemImpl::Interrupt(content::DownloadInterruptReason reason) {
  // Somewhat counter-intuitively, it is possible for us to receive an
  // interrupt after we've already been interrupted.  The generation of
  // interrupts from the file thread Renames and the generation of
  // interrupts from disk writes go through two different mechanisms (driven
  // by rename requests from UI thread and by write requests from IO thread,
  // respectively), and since we choose not to keep state on the File thread,
  // this is the place where the races collide.  It's also possible for
  // interrupts to race with cancels.

  // Whatever happens, the first one to hit the UI thread wins.
  if (!IsInProgress())
    return;

  last_reason_ = reason;
  TransitionTo(INTERRUPTED);
  download_stats::RecordDownloadInterrupted(
      reason, received_bytes_, total_bytes_);
  delegate_->DownloadStopped(this);
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

void DownloadItemImpl::OnAllDataSaved(
    int64 size, const std::string& final_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!all_data_saved_);
  all_data_saved_ = true;
  ProgressComplete(size, final_hash);
  UpdateObservers();
}

void DownloadItemImpl::MarkAsComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE);
}

void DownloadItemImpl::SetIsPersisted() {
  is_persisted_ = true;
  UpdateObservers();
}

void DownloadItemImpl::SetDbHandle(int64 handle) {
  db_handle_ = handle;

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_IN_HISTORY,
      net::NetLog::Int64Callback("db_handle", db_handle_));
}

// **** Download progression cascade

void DownloadItemImpl::Init(bool active,
                            download_net_logs::DownloadType download_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (active)
    download_stats::RecordDownloadCount(download_stats::START_COUNT);

  if (target_path_.empty())
    target_path_ = current_path_;
  std::string file_name;
  if (download_type == download_net_logs::SRC_HISTORY_IMPORT) {
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

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE,
      base::Bind(&download_net_logs::ItemActivatedCallback,
                 this, download_type, &file_name));

  // If this is not an active download, end the ACTIVE event now.
  if (!active) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_IN_HISTORY,
        net::NetLog::Int64Callback("db_handle", db_handle_));

    bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE);
  }

  VLOG(20) << __FUNCTION__ << "() " << DebugString(true);
}

// Called by DownloadManagerImpl when the download target path has been
// determined.
void DownloadItemImpl::OnDownloadTargetDetermined(
    const FilePath& target_path,
    TargetDisposition disposition,
    content::DownloadDangerType danger_type,
    const FilePath& intermediate_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the |target_path| is empty, then we consider this download to be
  // canceled.
  if (target_path.empty()) {
    Cancel(true);
    return;
  }

  target_path_ = target_path;
  target_disposition_ = disposition;
  SetDangerType(danger_type);
  // TODO(asanka): SetDangerType() doesn't need to send a notification here.

  // We want the intermediate and target paths to refer to the same directory so
  // that they are both on the same device and subject to same
  // space/permission/availability constraints.
  DCHECK(intermediate_path.DirName() == target_path.DirName());

  // Rename to intermediate name.
  // TODO(asanka): Skip this rename if AllDataSaved() is true. This avoids a
  //               spurious rename when we can just rename to the final
  //               filename. Unnecessary renames may cause bugs like
  //               http://crbug.com/74187.
  DownloadFileManager::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToIntermediateName,
                 weak_ptr_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::RenameDownloadFile,
                 delegate_->GetDownloadFileManager(), GetGlobalId(),
                 intermediate_path, false, callback));
}

void DownloadItemImpl::OnDownloadRenamedToIntermediateName(
    content::DownloadInterruptReason reason,
    const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (content::DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    Interrupt(reason);
  } else {
    SetFullPath(full_path);
    UpdateObservers();
  }

  delegate_->DownloadRenamedToIntermediateName(this);
}

void DownloadItemImpl::MaybeCompleteDownload() {
  // TODO(rdsmith): Move logic for this function here.
  delegate_->MaybeCompleteDownload(this);
}

// Called by DownloadManagerImpl::MaybeCompleteDownload() when it has
// determined that the download is ready for completion.
void DownloadItemImpl::OnDownloadCompleting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " needs rename = " << NeedsRename()
           << " " << DebugString(true);
  DCHECK(!GetTargetFilePath().empty());
  DCHECK_NE(DANGEROUS, GetSafetyState());

  if (NeedsRename()) {
    DownloadFileManager::RenameCompletionCallback callback =
        base::Bind(&DownloadItemImpl::OnDownloadRenamedToFinalName,
                   weak_ptr_factory_.GetWeakPtr());
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::RenameDownloadFile,
                   delegate_->GetDownloadFileManager(), GetGlobalId(),
                   GetTargetFilePath(), true, callback));
  } else {
    // Complete the download and release the DownloadFile.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::CompleteDownload,
                   delegate_->GetDownloadFileManager(), GetGlobalId(),
                   base::Bind(&DownloadItemImpl::OnDownloadFileReleased,
                              weak_ptr_factory_.GetWeakPtr())));
  }
}

void DownloadItemImpl::OnDownloadRenamedToFinalName(
    content::DownloadInterruptReason reason,
    const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "()"
           << " full_path = \"" << full_path.value() << "\""
           << " needed rename = " << NeedsRename()
           << " " << DebugString(false);
  DCHECK(NeedsRename());

  if (content::DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    Interrupt(reason);
    return;
  }

  // full_path is now the current and target file path.
  DCHECK(!full_path.empty());
  target_path_ = full_path;
  SetFullPath(full_path);
  delegate_->DownloadRenamedToFinalName(this);

  // Complete the download and release the DownloadFile.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::CompleteDownload,
                 delegate_->GetDownloadFileManager(), GetGlobalId(),
                 base::Bind(&DownloadItemImpl::OnDownloadFileReleased,
                            weak_ptr_factory_.GetWeakPtr())));
}

void DownloadItemImpl::OnDownloadFileReleased() {
  if (delegate_->ShouldOpenDownload(this))
    Completed();
  else
    delegate_delayed_complete_ = true;
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
  TransitionTo(COMPLETE);
  delegate_->DownloadCompleted(this);
  download_stats::RecordDownloadCompleted(start_tick_, received_bytes_);

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

bool DownloadItemImpl::NeedsRename() const {
  DCHECK(target_path_.DirName() == current_path_.DirName());
  return target_path_ != current_path_;
}

void DownloadItemImpl::ProgressComplete(int64 bytes_so_far,
                                        const std::string& final_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  hash_ = final_hash;
  hash_state_ = "";

  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;
}

void DownloadItemImpl::TransitionTo(DownloadState new_state) {
  if (state_ == new_state)
    return;

  DownloadState old_state = state_;
  state_ = new_state;

  switch (state_) {
    case COMPLETE:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_FINISHED,
          base::Bind(&download_net_logs::ItemFinishedCallback,
                     received_bytes_, &hash_));
      break;
    case INTERRUPTED:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_INTERRUPTED,
          base::Bind(&download_net_logs::ItemInterruptedCallback,
                     last_reason_, received_bytes_, &hash_state_));
      break;
    case CANCELLED:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_CANCELED,
          base::Bind(&download_net_logs::ItemCanceledCallback,
                     received_bytes_, &hash_state_));
      break;
    default:
      break;
  }

  VLOG(20) << " " << __FUNCTION__ << "()" << " this = " << DebugString(true);

  UpdateObservers();

  bool is_done = (state_ != IN_PROGRESS);
  bool was_done = (old_state != IN_PROGRESS);
  if (is_done && !was_done)
    bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE);
}

void DownloadItemImpl::SetDangerType(content::DownloadDangerType danger_type) {
  danger_type_ = danger_type;
  // Notify observers if the safety state has changed as a result of the new
  // danger type.
  SafetyState updated_value = IsDangerous() ?
      DownloadItem::DANGEROUS : DownloadItem::SAFE;
  if (updated_value != safety_state_) {
    safety_state_ = updated_value;
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
        base::Bind(&download_net_logs::ItemCheckedCallback,
                   GetDangerType(), GetSafetyState()));
  }
}

void DownloadItemImpl::SetFullPath(const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << "()"
           << " new_path = \"" << new_path.value() << "\""
           << " " << DebugString(true);
  DCHECK(!new_path.empty());
  current_path_ = new_path;

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_RENAMED,
      base::Bind(&download_net_logs::ItemRenamedCallback,
                 &current_path_, &new_path));
}




