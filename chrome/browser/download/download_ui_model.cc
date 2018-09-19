// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_model.h"

using safe_browsing::DownloadFileType;

DownloadUIModel::DownloadUIModel() = default;

DownloadUIModel::~DownloadUIModel() = default;

void DownloadUIModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadUIModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ContentId DownloadUIModel::GetContentId() const {
  NOTREACHED();
  return ContentId();
}

base::string16 DownloadUIModel::GetInterruptReasonText() const {
  return base::string16();
}

base::string16 DownloadUIModel::GetStatusText() const {
  return base::string16();
}

base::string16 DownloadUIModel::GetTabProgressStatusText() const {
  return base::string16();
}

base::string16 DownloadUIModel::GetTooltipText(const gfx::FontList& font_list,
                                               int max_width) const {
  return base::string16();
}

base::string16 DownloadUIModel::GetWarningText(const gfx::FontList& font_list,
                                               int base_width) const {
  return base::string16();
}

base::string16 DownloadUIModel::GetWarningConfirmButtonText() const {
  return base::string16();
}

int64_t DownloadUIModel::GetCompletedBytes() const {
  return 0;
}

int64_t DownloadUIModel::GetTotalBytes() const {
  return 0;
}

int DownloadUIModel::PercentComplete() const {
  return -1;
}

bool DownloadUIModel::IsDangerous() const {
  return false;
}

bool DownloadUIModel::MightBeMalicious() const {
  return false;
}

bool DownloadUIModel::IsMalicious() const {
  return false;
}

bool DownloadUIModel::HasSupportedImageMimeType() const {
  return false;
}

bool DownloadUIModel::ShouldAllowDownloadFeedback() const {
  return false;
}

bool DownloadUIModel::ShouldRemoveFromShelfWhenComplete() const {
  return false;
}

bool DownloadUIModel::ShouldShowDownloadStartedAnimation() const {
  return true;
}

bool DownloadUIModel::ShouldShowInShelf() const {
  return true;
}

void DownloadUIModel::SetShouldShowInShelf(bool should_show) {}

bool DownloadUIModel::ShouldNotifyUI() const {
  return true;
}

bool DownloadUIModel::WasUINotified() const {
  return false;
}

void DownloadUIModel::SetWasUINotified(bool should_notify) {}

bool DownloadUIModel::ShouldPreferOpeningInBrowser() const {
  return true;
}

void DownloadUIModel::SetShouldPreferOpeningInBrowser(bool preference) {}

DownloadFileType::DangerLevel DownloadUIModel::GetDangerLevel() const {
  return DownloadFileType::NOT_DANGEROUS;
}

void DownloadUIModel::SetDangerLevel(
    DownloadFileType::DangerLevel danger_level) {}

void DownloadUIModel::OpenUsingPlatformHandler() {}

bool DownloadUIModel::IsBeingRevived() const {
  return true;
}

void DownloadUIModel::SetIsBeingRevived(bool is_being_revived) {}

download::DownloadItem* DownloadUIModel::download() {
  return nullptr;
}

base::string16 DownloadUIModel::GetProgressSizesString() const {
  return base::string16();
}

base::FilePath DownloadUIModel::GetFileNameToReportUser() const {
  return base::FilePath();
}

base::FilePath DownloadUIModel::GetTargetFilePath() const {
  return base::FilePath();
}

void DownloadUIModel::OpenDownload() {}

download::DownloadItem::DownloadState DownloadUIModel::GetState() const {
  return download::DownloadItem::IN_PROGRESS;
}

bool DownloadUIModel::IsPaused() const {
  return false;
}

download::DownloadDangerType DownloadUIModel::GetDangerType() const {
  return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
}

bool DownloadUIModel::GetOpenWhenComplete() const {
  return false;
}

bool DownloadUIModel::TimeRemaining(base::TimeDelta* remaining) const {
  return false;
}

bool DownloadUIModel::GetOpened() const {
  return false;
}

void DownloadUIModel::SetOpened(bool opened) {}

bool DownloadUIModel::IsDone() const {
  return false;
}

void DownloadUIModel::Cancel(bool user_cancel) {}

void DownloadUIModel::SetOpenWhenComplete(bool open) {}

download::DownloadInterruptReason DownloadUIModel::GetLastReason() const {
  return download::DOWNLOAD_INTERRUPT_REASON_NONE;
}

base::FilePath DownloadUIModel::GetFullPath() const {
  return base::FilePath();
}

bool DownloadUIModel::CanResume() const {
  return false;
}

bool DownloadUIModel::AllDataSaved() const {
  return false;
}

bool DownloadUIModel::GetFileExternallyRemoved() const {
  return false;
}

GURL DownloadUIModel::GetURL() const {
  return GURL();
}

#if !defined(OS_ANDROID)
DownloadCommands DownloadUIModel::GetDownloadCommands() const {
  NOTREACHED();
  return DownloadCommands(nullptr);
}
#endif
