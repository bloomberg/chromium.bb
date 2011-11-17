// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_download_manager_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commdlg.h>
#endif

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_context.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_state_info.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
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
      "download");

  // Since we have no download UI, show the user a dialog always.
  state.prompt_user_for_save_location = true;

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
    state.suggested_path = download_manager_->BrowserContext()->GetPath().
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
  FilePath result;
#if defined(OS_WIN)
  std::wstring file_part = FilePath(suggested_path).BaseName().value();
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), arraysize(file_name));
  OPENFILENAME save_as;
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  save_as.hwndOwner = tab_contents->GetNativeView();
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = arraysize(file_name);

  std::wstring directory;
  if (!suggested_path.empty())
    directory = suggested_path.DirName().value();

  save_as.lpstrInitialDir = directory.c_str();
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

  if (GetSaveFileName(&save_as))
    result = FilePath(std::wstring(save_as.lpstrFile));
#else
  NOTIMPLEMENTED();
#endif

  if (result.empty()) {
    download_manager_->FileSelectionCanceled(data);
  } else {
    download_manager_->FileSelected(result, data);
  }
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

bool ShellDownloadManagerDelegate::ShouldCompleteDownload(DownloadItem* item) {
  return true;
}

bool ShellDownloadManagerDelegate::ShouldOpenDownload(DownloadItem* item) {
  return true;
}

bool ShellDownloadManagerDelegate::GenerateFileHash() {
  return false;
}

void ShellDownloadManagerDelegate::OnResponseCompleted(DownloadItem* item) {
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
