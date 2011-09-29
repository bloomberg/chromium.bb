// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_download_manager_delegate.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_context.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_state_info.h"
#include "net/base/net_util.h"

namespace content {

ShellDownloadManagerDelegate::ShellDownloadManagerDelegate()
    : download_manager_(NULL) {
}

ShellDownloadManagerDelegate::~ShellDownloadManagerDelegate(){
}


void ShellDownloadManagerDelegate::SetDownloadManager(
    DownloadManager* download_manager) {
  download_manager_ = download_manager;
}

void ShellDownloadManagerDelegate::Shutdown() {
}

bool ShellDownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  DownloadStateInfo state = download->state_info();

  if (!state.force_file_name.empty())
    return true;

  FilePath generated_name = net::GenerateFileName(
      download->GetURL(),
      download->content_disposition(),
      download->referrer_charset(),
      download->suggested_filename(),
      download->mime_type(),
      string16(UTF8ToUTF16("download")));

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ShellDownloadManagerDelegate::GenerateFilename,
          this, download_id, state, generated_name));
  return false;
}

void ShellDownloadManagerDelegate::GenerateFilename(
    int32 download_id,
    DownloadStateInfo state,
    const FilePath& generated_name) {
  if (state.suggested_path.empty()) {
    state.suggested_path = download_manager_->browser_context()->GetPath().
        Append(FILE_PATH_LITERAL("Downloads"));
    if (!file_util::PathExists(state.suggested_path))
      file_util::CreateDirectory(state.suggested_path);
  }

  state.suggested_path = state.suggested_path.Append(generated_name);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ShellDownloadManagerDelegate::RestartDownload,
          this, download_id, state));
}

void ShellDownloadManagerDelegate::RestartDownload(
    int32 download_id,
    DownloadStateInfo state) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;
  download->SetFileCheckResults(state);
  download_manager_->RestartDownload(download_id);
}

void ShellDownloadManagerDelegate::ChooseDownloadPath(
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* data) {
}

bool ShellDownloadManagerDelegate::OverrideIntermediatePath(
    DownloadItem* item,
    FilePath* intermediate_path) {
  return false;
}

TabContents* ShellDownloadManagerDelegate::
    GetAlternativeTabContentsToNotifyForDownload() {
  return NULL;
}

bool ShellDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const FilePath& path) {
  return false;
}

bool ShellDownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  return true;
}

bool ShellDownloadManagerDelegate::ShouldCompleteDownload(DownloadItem* item) {
  return true;
}

bool ShellDownloadManagerDelegate::GenerateFileHash() {
  return false;
}

void ShellDownloadManagerDelegate::OnResponseCompleted(
    DownloadItem* item,
    const std::string& hash) {
}

void ShellDownloadManagerDelegate::AddItemToPersistentStore(
    DownloadItem* item) {
}

void ShellDownloadManagerDelegate::UpdateItemInPersistentStore(
    DownloadItem* item) {
}

void ShellDownloadManagerDelegate::UpdatePathForItemInPersistentStore(
    DownloadItem* item,
    const FilePath& new_path) {
}

void ShellDownloadManagerDelegate::RemoveItemFromPersistentStore(
    DownloadItem* item) {
}

void ShellDownloadManagerDelegate::RemoveItemsFromPersistentStoreBetween(
    const base::Time remove_begin,
    const base::Time remove_end) {
}

void ShellDownloadManagerDelegate::GetSaveDir(
    TabContents* tab_contents,
    FilePath* website_save_dir,
    FilePath* download_save_dir) {
}

void ShellDownloadManagerDelegate::ChooseSavePath(
    const base::WeakPtr<SavePackage>& save_package,
    const FilePath& suggested_path,
    bool can_save_as_complete) {
}

void ShellDownloadManagerDelegate::DownloadProgressUpdated() {
}

}  // namespace content
