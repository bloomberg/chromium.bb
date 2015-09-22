// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_downloader.h"

#include "base/bind.h"
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

FileDownloader::FileDownloader(const GURL& url,
                               const base::FilePath& path,
                               bool overwrite,
                               net::URLRequestContextGetter* request_context,
                               const DownloadFinishedCallback& callback)
    : callback_(callback),
      fetcher_(URLFetcher::Create(url, URLFetcher::GET, this)),
      weak_ptr_factory_(this) {
  fetcher_->SetRequestContext(request_context);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  fetcher_->SaveResponseToFileAtPath(
      path,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

  if (overwrite) {
    fetcher_->Start();
  } else {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN).get(),
        FROM_HERE,
        base::Bind(&base::PathExists, path),
        base::Bind(&FileDownloader::OnFileExistsCheckDone,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

FileDownloader::~FileDownloader() {}

void FileDownloader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  // Delete |fetcher_| when we leave this method. This is necessary so the
  // download file will be deleted if the download failed.
  scoped_ptr<net::URLFetcher> fetcher(fetcher_.Pass());

  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "URLRequestStatus error " << status.error()
        << " while trying to download " << source->GetURL().spec();
    callback_.Run(false);
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code
        << " while trying to download " << source->GetURL().spec();
    callback_.Run(false);
    return;
  }

  // Take ownership of the new file.
  base::FilePath response_path;
  bool success = source->GetResponseAsFilePath(true, &response_path);
  callback_.Run(success);
}

void FileDownloader::OnFileExistsCheckDone(bool exists) {
  if (exists)
    callback_.Run(true);
  else
    fetcher_->Start();
}
