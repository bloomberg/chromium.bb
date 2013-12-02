// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crx_downloader.h"
#include "chrome/browser/component_updater/url_fetcher_downloader.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace component_updater {

// This factory method builds the chain of downloaders. Currently, there is only
// a url fetcher downloader but more downloaders can be chained up to handle
// the request.
CrxDownloader* CrxDownloader::Create(
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const DownloadCallback& download_callback) {
  CrxDownloader* crx_downloader =
      new UrlFetcherDownloader(context_getter, task_runner);

  crx_downloader->download_callback_ = download_callback;

  return crx_downloader;
}

CrxDownloader::CrxDownloader() : current_url_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CrxDownloader::~CrxDownloader() {
}

void CrxDownloader::StartDownloadFromUrl(const GURL& url) {
  std::vector<GURL> urls;
  urls.push_back(url);
  StartDownload(urls);
}

void CrxDownloader::StartDownload(const std::vector<GURL>& urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (urls.empty())
    return;

  urls_ = urls;

  current_url_ = 0;
  DoStartDownload(urls[current_url_]);
}

// Handles the fallback in the case of multiple urls and routing of the
// download to the following successor in the chain.
void CrxDownloader::OnDownloadComplete(bool is_handled,
                                       int error,
                                       const base::FilePath& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If an error has occured, try the next url if possible, then move on
  // to the successor in the chain, if the request has not been handled
  if (error) {
    ++current_url_;
    if (current_url_ != urls_.size()) {
      DoStartDownload(urls_[current_url_]);
      return;
    }

    if (!is_handled && successor_) {
      successor_->StartDownload(urls_);
      return;
    }
  }

  DCHECK(is_handled || !error || !successor_);

  download_callback_.Run(error, response);
}

}  // namespace component_updater

