// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/view_http_cache_job_factory.h"

#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"

namespace {

// A job subclass that dumps an HTTP cache entry.
class ViewHttpCacheJob : public URLRequestSimpleJob {
 public:
  explicit ViewHttpCacheJob(URLRequest* request)
      : URLRequestSimpleJob(request) {}

  // URLRequestSimpleJob methods:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

 private:
  ~ViewHttpCacheJob() {}
};

bool ViewHttpCacheJob::GetData(std::string* mime_type,
                               std::string* charset,
                               std::string* data) const {
  mime_type->assign("text/html");
  charset->assign("UTF-8");

  data->clear();

  std::string cache_key;
  cache_key =
      request_->url().spec().substr(strlen(chrome::kNetworkViewCacheURL));
  ViewCacheHelper::GetEntryInfoHTML(cache_key,
                                    request_->context(),
                                    chrome::kNetworkViewCacheURL,
                                    data);

  return true;
}

}  // namespace

// static
bool ViewHttpCacheJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(),
                         chrome::kNetworkViewCacheURL,
                         true /*case_sensitive*/);
}

// static
URLRequestJob* ViewHttpCacheJobFactory::CreateJobForRequest(
    URLRequest* request) {
  return new ViewHttpCacheJob(request);
}
