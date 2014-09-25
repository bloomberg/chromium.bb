// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist_downloader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;
using net::URLFetcher;

const int kNumRetries = 1;

SupervisedUserBlacklistDownloader::SupervisedUserBlacklistDownloader(
    const GURL& url,
    const base::FilePath& path,
    net::URLRequestContextGetter* request_context,
    const DownloadFinishedCallback& callback)
    : callback_(callback),
      fetcher_(URLFetcher::Create(url, URLFetcher::GET, this)),
      weak_ptr_factory_(this) {
  fetcher_->SetRequestContext(request_context);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_IS_DOWNLOAD);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  fetcher_->SaveResponseToFileAtPath(
      path,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&base::PathExists, path),
      base::Bind(&SupervisedUserBlacklistDownloader::OnFileExistsCheckDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

SupervisedUserBlacklistDownloader::~SupervisedUserBlacklistDownloader() {}

void SupervisedUserBlacklistDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);

  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "URLRequestStatus error " << status.error();
    callback_.Run(false);
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    callback_.Run(false);
    return;
  }

  // Take ownership of the new file.
  base::FilePath response_path;
  bool success = source->GetResponseAsFilePath(true, &response_path);
  callback_.Run(success);
}

void SupervisedUserBlacklistDownloader::OnFileExistsCheckDone(bool exists) {
  if (exists) {
    // TODO(treib): Figure out a strategy for updating the file.
    callback_.Run(true);
  } else {
    fetcher_->Start();
  }
}
