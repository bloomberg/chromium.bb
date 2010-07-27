// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item.h"

#include "base/logging.h"
#include "base/timer.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/common/extensions/extension.h"

namespace {

// Update frequency (milliseconds).
const int kUpdateTimeMs = 1000;

}  // namespace

// Constructor for reading from the history service.
DownloadItem::DownloadItem(DownloadManager* download_manager,
                           const DownloadCreateInfo& info)
    : id_(-1),
      full_path_(info.path),
      url_(info.url),
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
      original_name_(info.original_name),
      render_process_id_(-1),
      request_id_(-1),
      save_as_(false),
      is_otr_(false),
      is_extension_install_(info.is_extension_install),
      name_finalized_(false),
      is_temporary_(false),
      need_final_rename_(false) {
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
      referrer_url_(info.referrer_url),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(info.start_time),
      db_handle_(DownloadManager::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(info.is_dangerous ? DANGEROUS : SAFE),
      auto_opened_(false),
      original_name_(info.original_name),
      render_process_id_(info.child_id),
      request_id_(info.request_id),
      save_as_(info.prompt_user_for_save_location),
      is_otr_(is_otr),
      is_extension_install_(info.is_extension_install),
      name_finalized_(false),
      is_temporary_(!info.save_info.file_path.empty()),
      need_final_rename_(false) {
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
      referrer_url_(GURL()),
      mime_type_(std::string()),
      original_mime_type_(std::string()),
      total_bytes_(0),
      received_bytes_(0),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS),
      start_time_(base::Time::Now()),
      db_handle_(DownloadManager::kUninitializedHandle),
      download_manager_(download_manager),
      is_paused_(false),
      open_when_complete_(false),
      safety_state_(SAFE),
      auto_opened_(false),
      original_name_(FilePath()),
      render_process_id_(-1),
      request_id_(-1),
      save_as_(false),
      is_otr_(is_otr),
      is_extension_install_(false),
      name_finalized_(false),
      is_temporary_(false),
      need_final_rename_(false) {
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
  FilePath file_to_use = full_path();
  if (!original_name().value().empty())
    file_to_use = original_name();

  return !Extension::IsExtension(file_to_use) &&
         !download_manager_->IsExecutableFile(file_to_use);
}

bool DownloadItem::ShouldOpenFileBasedOnExtension() {
  return download_manager_->ShouldOpenFileBasedOnExtension(full_path());
}

void DownloadItem::OpenFilesBasedOnExtension(bool open) {
  return download_manager_->OpenFilesBasedOnExtension(full_path(), open);
}

void DownloadItem::OpenDownload() {
  if (state() == DownloadItem::IN_PROGRESS) {
    open_when_complete_ = !open_when_complete_;
  } else if (state() == DownloadItem::COMPLETE) {
    FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
    download_manager_->OpenDownload(this, NULL);
  }
}

void DownloadItem::ShowDownloadInShell() {
  download_manager_->ShowDownloadInShell(this);
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

void DownloadItem::Finished(int64 size) {
  state_ = COMPLETE;
  UpdateSize(size);
  StopProgressTimer();
}

void DownloadItem::Remove(bool delete_on_disk) {
  Cancel(true);
  state_ = REMOVING;
  if (delete_on_disk)
    download_manager_->DeleteDownload(full_path_);
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
  DCHECK(!full_path.empty());
  full_path_ = full_path;
  file_name_ = full_path_.BaseName();
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
  if (state() == DownloadItem::COMPLETE)
    NotifyObserversDownloadFileCompleted();
}

FilePath DownloadItem::GetFileName() const {
  if (safety_state_ == DownloadItem::SAFE)
    return file_name_;
  if (path_uniquifier_ > 0) {
    FilePath name(original_name_);
    download_util::AppendNumberToPath(&name, path_uniquifier_);
    return name;
  }
  return original_name_;
}

void DownloadItem::Init(bool start_timer) {
  file_name_ = full_path_.BaseName();
  if (start_timer)
    StartProgressTimer();
}
