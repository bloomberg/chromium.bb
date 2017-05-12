// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

// Class that will attempt to download the Chrome Cleaner executable and
// call a given callback when done. Instances of ChromeCleanerFetcher own
// themselves and will self-delete on completion of the network request.
class ChromeCleanerFetcher : public net::URLFetcherDelegate {
 public:
  explicit ChromeCleanerFetcher(ChromeCleanerFetchedCallback fetched_callback);

 protected:
  ~ChromeCleanerFetcher() override;

 private:
  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  ChromeCleanerFetchedCallback fetched_callback_;
  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerFetcher);
};

ChromeCleanerFetcher::ChromeCleanerFetcher(
    ChromeCleanerFetchedCallback fetched_callback)
    : fetched_callback_(std::move(fetched_callback)),
      url_fetcher_(net::URLFetcher::Create(0,
                                           GURL(GetSRTDownloadURL()),
                                           net::URLFetcher::GET,
                                           this)) {
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::SAFE_BROWSING);
  url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  url_fetcher_->SetMaxRetriesOn5xx(3);
  url_fetcher_->SaveResponseToTemporaryFile(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::FILE));
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher_->Start();
}

ChromeCleanerFetcher::~ChromeCleanerFetcher() = default;

void ChromeCleanerFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  // Take ownership of the fetcher in this scope (source == url_fetcher_).
  DCHECK_EQ(url_fetcher_.get(), source);

  base::FilePath download_path;
  if (source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK &&
      source->GetResponseAsFilePath(true, &download_path)) {
    DCHECK(!download_path.empty());
  }

  DCHECK(fetched_callback_);
  std::move(fetched_callback_)
      .Run(std::move(download_path), source->GetResponseCode());

  // Since url_fetcher_ is passed a pointer to this object during construction,
  // explicitly destroy the url_fetcher_ to avoid potential destruction races.
  url_fetcher_.reset();

  // At this point, the url_fetcher_ is gone and this ChromeCleanerFetcher
  // instance is no longer needed.
  delete this;
}

}  // namespace

void FetchChromeCleaner(ChromeCleanerFetchedCallback fetched_callback) {
  new ChromeCleanerFetcher(std::move(fetched_callback));
}

}  // namespace safe_browsing
