// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

class SyncUrlFetcher : public net::URLFetcherDelegate {
 public:
  SyncUrlFetcher() {}
  virtual ~SyncUrlFetcher() {}

  bool Fetch(const GURL& url,
             URLRequestContextGetter* getter,
             std::string* response) {
    MessageLoop loop;
    scoped_ptr<net::URLFetcher> fetcher_(
        net::URLFetcher::Create(url, net::URLFetcher::GET, this));
    fetcher_->SetRequestContext(getter);
    response_ = response;
    fetcher_->Start();
    loop.Run();
    return success_;
  }

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    success_ = (source->GetResponseCode() == 200);
    if (success_)
      success_ = source->GetResponseAsString(response_);
    MessageLoop::current()->Quit();
  }

 private:
  bool success_;
  std::string* response_;
};

}  // namespace

bool FetchUrl(const GURL& url,
              URLRequestContextGetter* getter,
              std::string* response) {
  return SyncUrlFetcher().Fetch(url, getter, response);
}
