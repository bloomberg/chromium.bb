// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/fake_download_item.h"

#include "base/bind.h"

using content::DownloadItem;

namespace test {

FakeDownloadItem::FakeDownloadItem() = default;

FakeDownloadItem::~FakeDownloadItem() {
  NotifyDownloadRemoved();
  NotifyDownloadDestroyed();
}

void FakeDownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeDownloadItem::NotifyDownloadDestroyed() {
  for (auto& observer : observers_) {
    observer.OnDownloadDestroyed(this);
  }
}

void FakeDownloadItem::NotifyDownloadRemoved() {
  for (auto& observer : observers_) {
    observer.OnDownloadRemoved(this);
  }
}

void FakeDownloadItem::NotifyDownloadUpdated() {
  UpdateObservers();
}

void FakeDownloadItem::UpdateObservers() {
  for (auto& observer : observers_) {
    observer.OnDownloadUpdated(this);
  }
}

void FakeDownloadItem::SetId(uint32_t id) {
  id_ = id;
}

uint32_t FakeDownloadItem::GetId() const {
  return id_;
}

void FakeDownloadItem::SetURL(const GURL& url) {
  url_ = url;
}

const GURL& FakeDownloadItem::GetURL() const {
  return url_;
}

void FakeDownloadItem::SetTargetFilePath(const base::FilePath& file_path) {
  file_path_ = file_path;
}

const base::FilePath& FakeDownloadItem::GetTargetFilePath() const {
  return file_path_;
}

void FakeDownloadItem::SetFileExternallyRemoved(
    bool is_file_externally_removed) {
  is_file_externally_removed_ = is_file_externally_removed;
}

bool FakeDownloadItem::GetFileExternallyRemoved() const {
  return is_file_externally_removed_;
}

void FakeDownloadItem::SetStartTime(const base::Time& start_time) {
  start_time_ = start_time;
}

base::Time FakeDownloadItem::GetStartTime() const {
  return start_time_;
}

void FakeDownloadItem::SetEndTime(const base::Time& end_time) {
  end_time_ = end_time;
}

base::Time FakeDownloadItem::GetEndTime() const {
  return end_time_;
}

void FakeDownloadItem::SetState(const DownloadState& state) {
  download_state_ = state;
}

DownloadItem::DownloadState FakeDownloadItem::GetState() const {
  return download_state_;
}

void FakeDownloadItem::SetMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
}

std::string FakeDownloadItem::GetMimeType() const {
  return mime_type_;
}

void FakeDownloadItem::SetOriginalUrl(const GURL& url) {
  original_url_ = url;
}

const GURL& FakeDownloadItem::GetOriginalUrl() const {
  return original_url_;
}

// The methods below are not supported and are not expected to be called.
void FakeDownloadItem::ValidateDangerousDownload() {
  NOTREACHED();
}

void FakeDownloadItem::StealDangerousDownload(
    bool delete_file_afterward,
    const AcquireFileCallback& callback) {
  NOTREACHED();
  callback.Run(base::FilePath());
}

void FakeDownloadItem::Pause() {
  NOTREACHED();
}

void FakeDownloadItem::Resume() {
  NOTREACHED();
}

void FakeDownloadItem::Cancel(bool user_cancel) {
  NOTREACHED();
}

void FakeDownloadItem::Remove() {
  NOTREACHED();
}

void FakeDownloadItem::OpenDownload() {
  NOTREACHED();
}

void FakeDownloadItem::ShowDownloadInShell() {
  NOTREACHED();
}

const std::string& FakeDownloadItem::GetGuid() const {
  NOTREACHED();
  return dummy_string;
}

content::DownloadInterruptReason FakeDownloadItem::GetLastReason() const {
  NOTREACHED();
  return content::DownloadInterruptReason();
}

bool FakeDownloadItem::IsPaused() const {
  NOTREACHED();
  return false;
}

bool FakeDownloadItem::IsTemporary() const {
  NOTREACHED();
  return false;
}

bool FakeDownloadItem::CanResume() const {
  NOTREACHED();
  return false;
}

bool FakeDownloadItem::IsDone() const {
  NOTREACHED();
  return true;
}

const std::vector<GURL>& FakeDownloadItem::GetUrlChain() const {
  NOTREACHED();
  return dummy_url_vector;
}

const GURL& FakeDownloadItem::GetReferrerUrl() const {
  NOTREACHED();
  return dummy_url;
}

const GURL& FakeDownloadItem::GetSiteUrl() const {
  NOTREACHED();
  return dummy_url;
}

const GURL& FakeDownloadItem::GetTabUrl() const {
  NOTREACHED();
  return dummy_url;
}

const GURL& FakeDownloadItem::GetTabReferrerUrl() const {
  NOTREACHED();
  return dummy_url;
}

std::string FakeDownloadItem::GetSuggestedFilename() const {
  NOTREACHED();
  return std::string();
}

std::string FakeDownloadItem::GetContentDisposition() const {
  NOTREACHED();
  return std::string();
}

std::string FakeDownloadItem::GetOriginalMimeType() const {
  NOTREACHED();
  return std::string();
}

std::string FakeDownloadItem::GetRemoteAddress() const {
  NOTREACHED();
  return std::string();
}

bool FakeDownloadItem::HasUserGesture() const {
  NOTREACHED();
  return false;
}

ui::PageTransition FakeDownloadItem::GetTransitionType() const {
  NOTREACHED();
  return ui::PageTransition();
}

const std::string& FakeDownloadItem::GetLastModifiedTime() const {
  NOTREACHED();
  return dummy_string;
}

const std::string& FakeDownloadItem::GetETag() const {
  NOTREACHED();
  return dummy_string;
}

bool FakeDownloadItem::IsSavePackageDownload() const {
  NOTREACHED();
  return false;
}

const base::FilePath& FakeDownloadItem::GetFullPath() const {
  NOTREACHED();
  return dummy_file_path;
}

const base::FilePath& FakeDownloadItem::GetForcedFilePath() const {
  NOTREACHED();
  return dummy_file_path;
}

base::FilePath FakeDownloadItem::GetFileNameToReportUser() const {
  NOTREACHED();
  return base::FilePath();
}

DownloadItem::TargetDisposition FakeDownloadItem::GetTargetDisposition() const {
  NOTREACHED();
  return TargetDisposition();
}

const std::string& FakeDownloadItem::GetHash() const {
  NOTREACHED();
  return dummy_string;
}

void FakeDownloadItem::DeleteFile(const base::Callback<void(bool)>& callback) {
  NOTREACHED();
  callback.Run(false);
}

bool FakeDownloadItem::IsDangerous() const {
  NOTREACHED();
  return false;
}

content::DownloadDangerType FakeDownloadItem::GetDangerType() const {
  NOTREACHED();
  return content::DownloadDangerType();
}

bool FakeDownloadItem::TimeRemaining(base::TimeDelta* remaining) const {
  NOTREACHED();
  return false;
}

int64_t FakeDownloadItem::CurrentSpeed() const {
  NOTREACHED();
  return 1;
}

int FakeDownloadItem::PercentComplete() const {
  NOTREACHED();
  return 1;
}

bool FakeDownloadItem::AllDataSaved() const {
  NOTREACHED();
  return true;
}

int64_t FakeDownloadItem::GetTotalBytes() const {
  NOTREACHED();
  return 1;
}

int64_t FakeDownloadItem::GetReceivedBytes() const {
  NOTREACHED();
  return 1;
}

bool FakeDownloadItem::CanShowInFolder() {
  NOTREACHED();
  return true;
}

bool FakeDownloadItem::CanOpenDownload() {
  NOTREACHED();
  return true;
}

bool FakeDownloadItem::ShouldOpenFileBasedOnExtension() {
  NOTREACHED();
  return true;
}

bool FakeDownloadItem::GetOpenWhenComplete() const {
  NOTREACHED();
  return false;
}

bool FakeDownloadItem::GetAutoOpened() {
  NOTREACHED();
  return false;
}

bool FakeDownloadItem::GetOpened() const {
  NOTREACHED();
  return false;
}

content::BrowserContext* FakeDownloadItem::GetBrowserContext() const {
  NOTREACHED();
  return nullptr;
}

content::WebContents* FakeDownloadItem::GetWebContents() const {
  NOTREACHED();
  return nullptr;
}

void FakeDownloadItem::OnContentCheckCompleted(
    content::DownloadDangerType danger_type) {
  NOTREACHED();
}

void FakeDownloadItem::SetOpenWhenComplete(bool open) {
  NOTREACHED();
}

void FakeDownloadItem::SetOpened(bool opened) {
  NOTREACHED();
}

void FakeDownloadItem::SetDisplayName(const base::FilePath& name) {
  NOTREACHED();
}

std::string FakeDownloadItem::DebugString(bool verbose) const {
  NOTREACHED();
  return std::string();
}

}  // namespace test
