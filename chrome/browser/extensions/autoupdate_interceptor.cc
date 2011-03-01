// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/autoupdate_interceptor.h"

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

// This is a specialized version of net::URLRequestTestJob that lets us specify
// response data and make sure the response code is 200, which the autoupdate
// code relies on.
class AutoUpdateTestRequestJob : public net::URLRequestTestJob {
 public:
  AutoUpdateTestRequestJob(net::URLRequest* request,
                           const std::string& response_data)
      : net::URLRequestTestJob(request,
                               net::URLRequestTestJob::test_headers(),
                               response_data,
                               true) {
  }

  virtual int GetResponseCode() const { return 200; }

 private:
  ~AutoUpdateTestRequestJob() {}
};


AutoUpdateInterceptor::AutoUpdateInterceptor() {
  net::URLRequest::RegisterRequestInterceptor(this);
}

AutoUpdateInterceptor::~AutoUpdateInterceptor() {
  net::URLRequest::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* AutoUpdateInterceptor::MaybeIntercept(
    net::URLRequest* request) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (request->url().scheme() != "http" ||
      request->url().host() != "localhost") {
      return NULL;
  }

  // It's ok to do a blocking disk access on this thread; this class
  // is just used for tests.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Search for this request's url, ignoring any query parameters.
  GURL url = request->url();
  if (url.has_query()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    url = url.ReplaceComponents(replacements);
  }
  std::map<GURL, FilePath>::iterator i = responses_.find(url);
  if (i == responses_.end()) {
    return NULL;
  }
  std::string contents;
  EXPECT_TRUE(file_util::ReadFileToString(i->second, &contents));

  return new AutoUpdateTestRequestJob(request, contents);
}


void AutoUpdateInterceptor::SetResponse(const std::string url,
                                        const FilePath& path) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // It's ok to do a blocking disk access on this thread; this class
  // is just used for tests.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  GURL gurl(url);
  EXPECT_EQ("http", gurl.scheme());
  EXPECT_EQ("localhost", gurl.host());
  EXPECT_TRUE(file_util::PathExists(path));
  responses_[gurl] = path;
}


void AutoUpdateInterceptor::SetResponseOnIOThread(const std::string url,
                                                  const FilePath& path) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &AutoUpdateInterceptor::SetResponse, url, path));
}
