// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates what wininet does when a dns lookup fails.

#ifndef CONTENT_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
#define CONTENT_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request_job.h"

class URLRequestFailedDnsJob : public net::URLRequestJob {
 public:
  explicit URLRequestFailedDnsJob(net::URLRequest* request);

  virtual void Start() OVERRIDE;

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // A test URL that can be used in UI tests.
  CONTENT_EXPORT static const char kTestUrl[];

  // Adds the testing URLs to the net::URLRequestFilter.
  CONTENT_EXPORT static void AddUrlHandler();

 private:
  virtual ~URLRequestFailedDnsJob();

  // Simulate a DNS failure.
  void StartAsync();

  base::WeakPtrFactory<URLRequestFailedDnsJob> weak_factory_;
};

#endif  // CONTENT_BROWSER_NET_URL_REQUEST_FAILED_DNS_JOB_H_
