// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A URLRequestMockHTTPJob class that inserts a time delay in processing.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_SLOW_HTTP_JOB_H_
#define CHROME_BROWSER_NET_URL_REQUEST_SLOW_HTTP_JOB_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/net/url_request_mock_http_job.h"

class URLRequestSlowHTTPJob : public URLRequestMockHTTPJob {
 public:
  URLRequestSlowHTTPJob(net::URLRequest* request, const FilePath& file_path);

  static const int kDelayMs;

  static net::URLRequest::ProtocolFactory Factory;

  // Adds the testing URLs to the net::URLRequestFilter.
  static void AddUrlHandler(const FilePath& base_path);

  // Given the path to a file relative to base_path_, construct a mock URL.
  static GURL GetMockUrl(const FilePath& path);

  virtual void Start();

 private:
  ~URLRequestSlowHTTPJob();

  void RealStart();

  base::OneShotTimer<URLRequestSlowHTTPJob> delay_timer_;

  // This is the file path leading to the root of the directory to use as the
  // root of the http server.
  static FilePath base_path_;
};

#endif  // CHROME_BROWSER_NET_URL_REQUEST_SLOW_HTTP_JOB_H_
