// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace media_router {

// Used to make a single HTTP GET request with |app_url| to fetch an app info
// from a DIAL device.  If successful, |success_cb| is invoked with the result;
// otherwise, |error_cb| is invoked with an error reason.
// This class is not thread safe and should be invoked .
class DialAppInfoFetcher : public net::URLFetcherDelegate {
 public:
  // Used to identify the net::URLFetcher instance for tests.
  static constexpr int kURLFetcherIDForTest = 1;

  // |request_context| is unowned; the caller must ensure that this object does
  // not outlive it.
  DialAppInfoFetcher(
      const GURL& app_url,
      net::URLRequestContextGetter* request_context,
      base::OnceCallback<void(const std::string&)> success_cb,
      base::OnceCallback<void(int, const std::string&)> error_cb);

  ~DialAppInfoFetcher() override;

  const GURL& app_url() { return app_url_; }

  virtual void Start();

 protected:
  // Runs |error_cb_| with |message| and clears it.
  void ReportError(int response_code, const std::string& message);

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override {}
  void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                int64_t current,
                                int64_t total) override {}

  const GURL app_url_;
  const scoped_refptr<net::URLRequestContextGetter> request_context_;
  base::OnceCallback<void(const std::string&)> success_cb_;
  base::OnceCallback<void(int, const std::string&)> error_cb_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_
