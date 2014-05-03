// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_download_manager_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commdlg.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace content {

ShellDownloadManagerDelegate::ShellDownloadManagerDelegate()
    : download_manager_(NULL),
      suppress_prompting_(false),
      weak_ptr_factory_(this) {}

ShellDownloadManagerDelegate::~ShellDownloadManagerDelegate(){
  if (download_manager_) {
    DCHECK_EQ(static_cast<DownloadManagerDelegate*>(this),
              download_manager_->GetDelegate());
    download_manager_->SetDelegate(NULL);
    download_manager_ = NULL;
  }
}


void ShellDownloadManagerDelegate::SetDownloadManager(
    DownloadManager* download_manager) {
  download_manager_ = download_manager;
}

void ShellDownloadManagerDelegate::Shutdown() {
  // Revoke any pending callbacks. download_manager_ et. al. are no longer safe
  // to access after this point.
  weak_ptr_factory_.InvalidateWeakPtrs();
  download_manager_ = NULL;
}

bool ShellDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* download,
    const DownloadTargetCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This assignment needs to be here because even at the call to
  // SetDownloadManager, the system is not fully initialized.
  if (default_download_path_.empty()) {
    default_download_path_ = download_manager_->GetBrowserContext()->GetPath().
        Append(FILE_PATH_LITERAL("Downloads"));
  }

  if (!download->GetForcedFilePath().empty()) {
    callback.Run(download->GetForcedFilePath(),
                 DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 download->GetForcedFilePath());
    return true;
  }

  FilenameDeterminedCallback filename_determined_callback =
      base::Bind(&ShellDownloadManagerDelegate::OnDownloadPathGenerated,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId(),
                 callback);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ShellDownloadManagerDelegate::GenerateFilename,
                 download->GetURL(),
                 download->GetContentDisposition(),
                 download->GetSuggestedFilename(),
                 download->GetMimeType(),
                 default_download_path_,
                 filename_determined_callback));
  return true;
}

bool ShellDownloadManagerDelegate::ShouldOpenDownload(
      DownloadItem* item,
      const DownloadOpenDelayedCallback& callback) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree) &&
      WebKitTestController::Get()->IsMainWindow(item->GetWebContents()) &&
      item->GetMimeType() == "text/html") {
    WebKitTestController::Get()->OpenURL(
        net::FilePathToFileURL(item->GetFullPath()));
  }
  return true;
}

void ShellDownloadManagerDelegate::GetNextId(
    const DownloadIdCallback& callback) {
  static uint32 next_id = DownloadItem::kInvalidId + 1;
  callback.Run(next_id++);
}

// static
void ShellDownloadManagerDelegate::GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    const FilenameDeterminedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath generated_name = net::GenerateFileName(url,
                                                        content_disposition,
                                                        std::string(),
                                                        suggested_filename,
                                                        mime_type,
                                                        "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, suggested_path));
}

void ShellDownloadManagerDelegate::OnDownloadPathGenerated(
    uint32 download_id,
    const DownloadTargetCallback& callback,
    const base::FilePath& suggested_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (suppress_prompting_) {
    // Testing exit.
    callback.Run(suggested_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload")));
    return;
  }

  ChooseDownloadPath(download_id, callback, suggested_path);
}

void ShellDownloadManagerDelegate::ChooseDownloadPath(
    uint32 download_id,
    const DownloadTargetCallback& callback,
    const base::FilePath& suggested_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != DownloadItem::IN_PROGRESS))
    return;

  base::FilePath result;
#if defined(OS_WIN)
  std::wstring file_part = base::FilePath(suggested_path).BaseName().value();
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), arraysize(file_name));
  OPENFILENAME save_as;
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  save_as.hwndOwner = item->GetWebContents()->GetNativeView()->
      GetHost()->GetAcceleratedWidget();
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = arraysize(file_name);

  std::wstring directory;
  if (!suggested_path.empty())
    directory = suggested_path.DirName().value();

  save_as.lpstrInitialDir = directory.c_str();
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

  if (GetSaveFileName(&save_as))
    result = base::FilePath(std::wstring(save_as.lpstrFile));
#else
  NOTIMPLEMENTED();
#endif

  callback.Run(result, DownloadItem::TARGET_DISPOSITION_PROMPT,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, result);
}

void ShellDownloadManagerDelegate::SetDownloadBehaviorForTesting(
    const base::FilePath& default_download_path) {
  default_download_path_ = default_download_path;
  suppress_prompting_ = true;
}

}  // namespace content
