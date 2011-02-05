// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_download_helper.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/file_util.h"
#include "chrome/browser/net/url_request_tracking.h"
#include "net/base/io_buffer.h"

PluginDownloadUrlHelper::PluginDownloadUrlHelper(
    const std::string& download_url,
    int source_child_unique_id,
    gfx::NativeWindow caller_window,
    PluginDownloadUrlHelper::DownloadDelegate* delegate)
    : download_file_request_(NULL),
      download_file_buffer_(new net::IOBuffer(kDownloadFileBufferSize)),
      download_file_caller_window_(caller_window),
      download_url_(download_url),
      download_source_child_unique_id_(source_child_unique_id),
      delegate_(delegate) {
  memset(download_file_buffer_->data(), 0, kDownloadFileBufferSize);
  download_file_.reset(new net::FileStream());
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
  if (download_file_request_) {
    delete download_file_request_;
    download_file_request_ = NULL;
  }
}

void PluginDownloadUrlHelper::InitiateDownload(
    net::URLRequestContext* request_context) {
  download_file_request_ = new net::URLRequest(GURL(download_url_), this);
  chrome_browser_net::SetOriginPIDForRequest(
      download_source_child_unique_id_, download_file_request_);
  download_file_request_->set_context(request_context);
  download_file_request_->Start();
}

void PluginDownloadUrlHelper::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  net::URLRequest::Delegate::OnAuthRequired(request, auth_info);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnSSLCertificateError(
    net::URLRequest* request,
    int cert_error,
    net::X509Certificate* cert) {
  net::URLRequest::Delegate::OnSSLCertificateError(request, cert_error, cert);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnResponseStarted(net::URLRequest* request) {
  if (!download_file_->IsOpen()) {
    // This is safe because once the temp file has been safely created, an
    // attacker can't drop a symlink etc into place.
    file_util::CreateTemporaryFile(&download_file_path_);
    download_file_->Open(download_file_path_,
                         base::PLATFORM_FILE_CREATE_ALWAYS |
                         base::PLATFORM_FILE_READ | base::PLATFORM_FILE_WRITE);
    if (!download_file_->IsOpen()) {
      NOTREACHED();
      OnDownloadCompleted(request);
      return;
    }
  }
  if (!request->status().is_success()) {
    OnDownloadCompleted(request);
  } else {
    // Initiate a read.
    int bytes_read = 0;
    if (!request->Read(download_file_buffer_, kDownloadFileBufferSize,
                       &bytes_read)) {
      // If the error is not an IO pending, then we're done
      // reading.
      if (!request->status().is_io_pending()) {
        OnDownloadCompleted(request);
      }
    } else if (bytes_read == 0) {
      OnDownloadCompleted(request);
    } else {
      OnReadCompleted(request, bytes_read);
    }
  }
}

void PluginDownloadUrlHelper::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  DCHECK(download_file_->IsOpen());

  if (bytes_read == 0) {
    OnDownloadCompleted(request);
    return;
  }

  int request_bytes_read = bytes_read;

  while (request->status().is_success()) {
    int bytes_written = download_file_->Write(download_file_buffer_->data(),
        request_bytes_read, NULL);
    DCHECK((bytes_written < 0) || (bytes_written == request_bytes_read));

    if ((bytes_written < 0) || (bytes_written != request_bytes_read)) {
      DownloadCompletedHelper(false);
      break;
    }

    // Start reading
    request_bytes_read = 0;
    if (!request->Read(download_file_buffer_, kDownloadFileBufferSize,
                       &request_bytes_read)) {
      if (!request->status().is_io_pending()) {
        // If the error is not an IO pending, then we're done
        // reading.
        OnDownloadCompleted(request);
      }
      break;
    } else if (request_bytes_read == 0) {
      OnDownloadCompleted(request);
      break;
    }
  }
}

void PluginDownloadUrlHelper::OnDownloadCompleted(net::URLRequest* request) {
  bool success = true;
  if (!request->status().is_success()) {
    success = false;
  } else if (!download_file_->IsOpen()) {
    success = false;
  }

  DownloadCompletedHelper(success);
}

void PluginDownloadUrlHelper::DownloadCompletedHelper(bool success) {
  if (download_file_->IsOpen()) {
      download_file_.reset();
  }

  if (success) {
    FilePath new_download_file_path =
      download_file_path_.DirName().AppendASCII(
          download_file_request_->url().ExtractFileName());

    file_util::Delete(new_download_file_path, false);

    if (!file_util::ReplaceFileW(download_file_path_,
                                 new_download_file_path)) {
      DLOG(ERROR) << "Failed to rename file:"
                  << download_file_path_.value()
                  << " to file:"
                  << new_download_file_path.value();
    } else {
      download_file_path_ = new_download_file_path;
    }
  }

  if (delegate_) {
    delegate_->OnDownloadCompleted(download_file_path_, success);
  } else {
    std::wstring path = download_file_path_.value();
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

#endif  // OS_WIN
