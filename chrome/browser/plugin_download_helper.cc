// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_download_helper.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include <windows.h>

#include "base/file_util.h"
#include "content/browser/browser_thread.h"
#include "net/base/io_buffer.h"

PluginDownloadUrlHelper::PluginDownloadUrlHelper(
    const std::string& download_url,
    gfx::NativeWindow caller_window,
    PluginDownloadUrlHelper::DownloadDelegate* delegate)
    : download_file_fetcher_(NULL),
      download_file_caller_window_(caller_window),
      download_url_(download_url),
      delegate_(delegate) {
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
}

void PluginDownloadUrlHelper::InitiateDownload(
    net::URLRequestContextGetter* request_context,
    base::MessageLoopProxy* file_thread_proxy) {
  download_file_fetcher_.reset(
      new URLFetcher(GURL(download_url_), URLFetcher::GET, this));
  download_file_fetcher_->set_request_context(request_context);
  download_file_fetcher_->SaveResponseToTemporaryFile(file_thread_proxy);
  download_file_fetcher_->Start();
}

void PluginDownloadUrlHelper::OnURLFetchComplete(const URLFetcher* source) {
  bool success = source->status().is_success();
  FilePath response_file;

  if (success) {
    if (source->GetResponseAsFilePath(true, &response_file)) {
      FilePath new_download_file_path =
          response_file.DirName().AppendASCII(
              download_file_fetcher_->url().ExtractFileName());

      file_util::Delete(new_download_file_path, false);

      if (!file_util::ReplaceFileW(response_file,
                                   new_download_file_path)) {
        DLOG(ERROR) << "Failed to rename file:"
                    << response_file.value()
                    << " to file:"
                    << new_download_file_path.value();
      } else {
        response_file = new_download_file_path;
      }
    } else {
      NOTREACHED() << "Failed to download the plugin installer.";
      success = false;
    }
  }

  if (delegate_) {
    delegate_->OnDownloadCompleted(response_file, success);
  } else {
    std::wstring path = response_file.value();
    COPYDATASTRUCT download_file_data = {0};
    download_file_data.cbData =
        static_cast<unsigned long>((path.length() + 1) * sizeof(wchar_t));
    download_file_data.lpData = const_cast<wchar_t *>(path.c_str());
    download_file_data.dwData = success;

    if (::IsWindow(download_file_caller_window_)) {
      ::SendMessage(download_file_caller_window_, WM_COPYDATA, NULL,
                    reinterpret_cast<LPARAM>(&download_file_data));
    }
  }
  // Don't access any members after this.
  delete this;
}

#endif  // defined(OS_WIN) && !defined(USE_AURA)
