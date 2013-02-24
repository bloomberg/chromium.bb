// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <shobjidl.h>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/metro.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "win8/util/win8_util.h"

// This code doesn't compile with Aura on. TODO(avi): hook it up so that
// win_aura can do platform integration.
#if !defined(USE_AURA)

namespace {

const char kDownloadNotificationPrefix[] = "DownloadNotification";
int g_next_notification_id = 0;

void UpdateTaskbarProgressBar(int download_count,
                              bool progress_known,
                              float progress) {
  // Taskbar progress bar is only supported on Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  base::win::ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result)) {
    VLOG(1) << "Failed creating a TaskbarList object: " << result;
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed initializing an ITaskbarList3 interface.";
    return;
  }

  // Iterate through all the browser windows, and draw the progress bar.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    BrowserWindow* window = browser->window();
    if (!window)
      continue;
    HWND frame = window->GetNativeWindow();
    if (download_count == 0 || progress == 1.0f)
      taskbar->SetProgressState(frame, TBPF_NOPROGRESS);
    else if (!progress_known)
      taskbar->SetProgressState(frame, TBPF_INDETERMINATE);
    else
      taskbar->SetProgressValue(frame, static_cast<int>(progress * 100), 100);
  }
}

void MetroDownloadNotificationClickedHandler(const wchar_t* download_path) {
  // Metro chrome will invoke these handlers on the metro thread.
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ensure that we invoke the function to display the downloaded item on the
  // UI thread.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(platform_util::ShowItemInFolder,
                                              base::FilePath(download_path)));
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    content::DownloadItem* download) {

  // Always update overall progress.
  float progress = 0;
  int download_count = 0;
  bool progress_known = GetProgress(&progress, &download_count);
  UpdateTaskbarProgressBar(download_count, progress_known, progress);

  // Fire notifications when downloads complete.
  if (!win8::IsSingleWindowMetroMode())
    return;

  if (download->GetState() != content::DownloadItem::COMPLETE)
    return;

  if (download->GetOpenWhenComplete() ||
      download->ShouldOpenFileBasedOnExtension() ||
      download->IsTemporary() ||
      download->GetAutoOpened())
    return;

  // Don't display the Windows8 metro notifications for an incognito download.
  if (download->GetBrowserContext() &&
      download->GetBrowserContext()->IsOffTheRecord())
    return;

  // Don't display the Windows 8 metro notifications if we are in the
  // foreground.
  HWND foreground_window = ::GetForegroundWindow();
  if (::IsWindow(foreground_window)) {
    DWORD process_id = 0;
    ::GetWindowThreadProcessId(foreground_window, &process_id);
    if (process_id == ::GetCurrentProcessId())
      return;
  }

  // In Windows 8 metro mode display a metro style notification which
  // informs the user that the download is complete.
  HMODULE metro = base::win::GetMetroModule();
  base::win::MetroNotification display_notification =
      reinterpret_cast<base::win::MetroNotification>(
          ::GetProcAddress(metro, "DisplayNotification"));
  DCHECK(display_notification);
  if (display_notification) {
    string16 title = l10n_util::GetStringUTF16(
        IDS_METRO_DOWNLOAD_COMPLETE_NOTIFICATION_TITLE);
    string16 body = l10n_util::GetStringUTF16(
        IDS_METRO_DOWNLOAD_COMPLETE_NOTIFICATION);

    // Dummy notification id. Every metro style notification needs a
    // unique notification id.
    std::string notification_id = kDownloadNotificationPrefix;
    notification_id += base::IntToString(g_next_notification_id++);

    display_notification(download->GetURL().spec().c_str(),
                         "",
                         title.c_str(),
                         body.c_str(),
                         L"",
                         notification_id.c_str(),
                         MetroDownloadNotificationClickedHandler,
                         download->GetTargetFilePath().value().c_str());
  }
}

#endif  // !USE_AURA
