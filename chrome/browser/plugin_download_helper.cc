// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_download_helper.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

PluginDownloadUrlHelper::PluginDownloadUrlHelper() {
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
}

void PluginDownloadUrlHelper::InitiateDownload(
    const GURL& download_url,
    net::URLRequestContextGetter* request_context,
    const DownloadFinishedCallback& callback) {
  download_url_ = download_url;
  callback_ = callback;
  download_file_fetcher_.reset(content::URLFetcher::Create(
      download_url_, content::URLFetcher::GET, this));
  download_file_fetcher_->SetRequestContext(request_context);
  download_file_fetcher_->SaveResponseToTemporaryFile(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  download_file_fetcher_->Start();
}

void PluginDownloadUrlHelper::OnURLFetchComplete(
    const content::URLFetcher* source) {
  net::URLRequestStatus status = source->GetStatus();
  if (status.is_success()) {
    bool success = source->GetResponseAsFilePath(true, &downloaded_file_);
    DCHECK(success);
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&PluginDownloadUrlHelper::RenameDownloadedFile,
                   base::Unretained(this)),
        base::Bind(&PluginDownloadUrlHelper::RunCallback,
                   base::Unretained(this)));
  } else {
    NOTREACHED() << "Failed to download the plugin installer: "
                 << net::ErrorToString(status.error());
    RunCallback();
  }
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

void PluginDownloadUrlHelper::RunCallback() {
  callback_.Run(downloaded_file_);
  delete this;
}
