// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_manager_delegate.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"

MockDownloadManagerDelegate::MockDownloadManagerDelegate() {
}

MockDownloadManagerDelegate::~MockDownloadManagerDelegate() {
}

void MockDownloadManagerDelegate::SetDownloadManager(DownloadManager* dm) {
  download_manager_ = dm;
}

void MockDownloadManagerDelegate::Shutdown() {
}

bool MockDownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  return true;
}

void MockDownloadManagerDelegate::ChooseDownloadPath(
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* data) {
}

bool MockDownloadManagerDelegate::OverrideIntermediatePath(
    DownloadItem* item,
    FilePath* intermediate_path) {
  return false;
}

TabContents* MockDownloadManagerDelegate::
    GetAlternativeTabContentsToNotifyForDownload() {
  return NULL;
}

bool MockDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  return false;
}

bool MockDownloadManagerDelegate::ShouldCompleteDownload(DownloadItem* item) {
  return true;
}

bool MockDownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  return true;
}

bool MockDownloadManagerDelegate::GenerateFileHash() {
  return false;
}

void MockDownloadManagerDelegate::OnResponseCompleted(DownloadItem* item) {
}

void MockDownloadManagerDelegate::AddItemToPersistentStore(DownloadItem* item) {
  download_manager_->OnItemAddedToPersistentStore(item->GetId(), item->GetId());
}

void MockDownloadManagerDelegate::UpdateItemInPersistentStore(
    DownloadItem* item) {
}

void MockDownloadManagerDelegate::UpdatePathForItemInPersistentStore(
    DownloadItem* item,
    const FilePath& new_path) {
}

void MockDownloadManagerDelegate::RemoveItemFromPersistentStore(
    DownloadItem* item) {
}

void MockDownloadManagerDelegate::RemoveItemsFromPersistentStoreBetween(
    const base::Time remove_begin,
    const base::Time remove_end) {
}

void MockDownloadManagerDelegate::GetSaveDir(
    TabContents* tab_contents,
    FilePath* website_save_dir,
    FilePath* download_save_dir) {
}

void MockDownloadManagerDelegate::ChooseSavePath(
    const base::WeakPtr<SavePackage>& save_package,
    const FilePath& suggested_path,
    bool can_save_as_complete) {
}

void MockDownloadManagerDelegate::DownloadProgressUpdated() {
}
