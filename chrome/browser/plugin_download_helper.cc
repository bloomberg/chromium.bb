// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_download_helper.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;
using content::URLFetcher;

PluginDownloadUrlHelper::PluginDownloadUrlHelper() {
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
}

void PluginDownloadUrlHelper::InitiateDownload(
    const GURL& download_url,
    net::URLRequestContextGetter* request_context,
    const DownloadFinishedCallback& finished_callback,
    const ErrorCallback& error_callback) {
  download_url_ = download_url;
  download_finished_callback_ = finished_callback;
  error_callback_ = error_callback;
  download_file_fetcher_.reset(URLFetcher::Create(
      download_url_, URLFetcher::GET, this));
  download_file_fetcher_->SetRequestContext(request_context);
  download_file_fetcher_->SaveResponseToTemporaryFile(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  download_file_fetcher_->Start();
}

void PluginDownloadUrlHelper::OnURLFetchComplete(const URLFetcher* source) {
  net::URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    RunErrorCallback(base::StringPrintf("Error %d: %s",
                                        status.error(),
                                        net::ErrorToString(status.error())));
    return;
  }
  int response_code = source->GetResponseCode();
  if (response_code != 200 &&
      response_code != URLFetcher::RESPONSE_CODE_INVALID) {
    // If we don't get a HTTP response code, the URL request either failed
    // (which should be covered by the status check above) or the fetched URL
    // was a file: URL (in unit tests for example), in which case it's fine.
    RunErrorCallback(base::StringPrintf("HTTP status %d", response_code));
    return;
  }
  bool success = source->GetResponseAsFilePath(true, &downloaded_file_);
  DCHECK(success);
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PluginDownloadUrlHelper::RenameDownloadedFile,
                 base::Unretained(this)),
      base::Bind(&PluginDownloadUrlHelper::RunFinishedCallback,
                 base::Unretained(this)));
}

void PluginDownloadUrlHelper::RenameDownloadedFile() {
  FilePath new_download_file_path =
      downloaded_file_.DirName().AppendASCII(
          download_file_fetcher_->GetURL().ExtractFileName());

  file_util::Delete(new_download_file_path, false);

  if (file_util::ReplaceFile(downloaded_file_,
                             new_download_file_path)) {
    downloaded_file_ = new_download_file_path;
  } else {
    DPLOG(ERROR) << "Failed to rename file: "
                 << downloaded_file_.value()
                 << " to file: "
                 << new_download_file_path.value();
  }
}

void PluginDownloadUrlHelper::RunFinishedCallback() {
  download_finished_callback_.Run(downloaded_file_);
  delete this;
}

void PluginDownloadUrlHelper::RunErrorCallback(const std::string& msg) {
  error_callback_.Run(msg);
  delete this;
}
