// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates what wininet does when a dns lookup fails.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
#define CHROME_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
#pragma once

#include "base/task.h"
#include "net/url_request/url_request_job.h"

class URLRequestFailedDnsJob : public net::URLRequestJob {
 public:
  explicit URLRequestFailedDnsJob(net::URLRequest* request);

  virtual void Start();

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // A test URL that can be used in UI tests.
  static const char kTestUrl[];

  // Adds the testing URLs to the net::URLRequestFilter.
  static void AddUrlHandler();

 private:
  ~URLRequestFailedDnsJob();

  // Simulate a DNS failure.
  void StartAsync();

  ScopedRunnableMethodFactory<URLRequestFailedDnsJob> method_factory_;
};

#endif  // CHROME_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
