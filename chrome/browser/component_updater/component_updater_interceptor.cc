// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/component_updater/component_updater_interceptor.h"

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

ComponentUpdateInterceptor::ComponentUpdateInterceptor()
  : hit_count_(0) {
  net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
}

ComponentUpdateInterceptor::~ComponentUpdateInterceptor() {
  net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* ComponentUpdateInterceptor::MaybeIntercept(
    net::URLRequest* request) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (request->url().scheme() != "http" ||
      request->url().host() != "localhost") {
      return NULL;
  }

  // It's ok to do a blocking disk access on this thread; this class
  // is just used for tests.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  ResponseMap::iterator it = responses_.find(request->url());
  if (it == responses_.end()) {
    return NULL;
  }
  const Response& response = it->second;
  ++hit_count_;

  std::string contents;
  EXPECT_TRUE(file_util::ReadFileToString(response.data_path, &contents));

  return new net::URLRequestTestJob(request,
                                    response.headers,
                                    contents,
                                    true);
}

void ComponentUpdateInterceptor::SetResponse(const std::string& url,
                                             const std::string& headers,
                                             const FilePath& path) {
  // It's ok to do a blocking disk access on this thread; this class
  // is just used for tests.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  GURL gurl(url);
  EXPECT_EQ("http", gurl.scheme());
  EXPECT_EQ("localhost", gurl.host());
  EXPECT_TRUE(file_util::PathExists(path));
  Response response = { path, headers };
  responses_[gurl] = response;
}
