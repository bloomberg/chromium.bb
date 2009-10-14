// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/autoupdate_interceptor.h"

#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

// This is a specialized version of URLRequestTestJob that lets us specify
// response data and make sure the response code is 200, which the autoupdate
// code relies on.
class AutoUpdateTestRequestJob : public URLRequestTestJob {
 public:
  AutoUpdateTestRequestJob(URLRequest* request,
    const std::string& response_data) : URLRequestTestJob(
    request, URLRequestTestJob::test_headers(), response_data, true) {}
  virtual int GetResponseCode() const { return 200; }
};


AutoUpdateInterceptor::AutoUpdateInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

AutoUpdateInterceptor::~AutoUpdateInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}


URLRequestJob* AutoUpdateInterceptor::MaybeIntercept(URLRequest* request) {
  EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (request->url().scheme() != "http" ||
      request->url().host() != "localhost") {
      return NULL;
  }

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
  EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::IO));
  GURL gurl(url);
  EXPECT_EQ("http", gurl.scheme());
  EXPECT_EQ("localhost", gurl.host());
  EXPECT_TRUE(file_util::PathExists(path));
  responses_[gurl] = path;
}


void AutoUpdateInterceptor::SetResponseOnIOThread(const std::string url,
                                                  const FilePath& path) {
  MessageLoop* io_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  io_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AutoUpdateInterceptor::SetResponse,
      url, path));
}
