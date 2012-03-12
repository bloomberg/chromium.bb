// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A net::URLRequestJob class that substitutes LinkDoctor requests.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_
#define CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_
#pragma once

#include "content/test/net/url_request_mock_http_job.h"

class URLRequestMockLinkDoctorJob : public URLRequestMockHTTPJob {
 public:
  explicit URLRequestMockLinkDoctorJob(net::URLRequest* request);

  static net::URLRequest::ProtocolFactory Factory;

  // Adds the testing URLs to the net::URLRequestFilter.
  static void AddUrlHandler();

 private:
  virtual ~URLRequestMockLinkDoctorJob() {}
};

#endif  // CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_
