// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A URLRequestJob class that substitutes LinkDoctor requests.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_
#define CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_

#include "chrome/browser/net/url_request_mock_http_job.h"

class URLRequestMockLinkDoctorJob : public URLRequestMockHTTPJob {
 public:
  URLRequestMockLinkDoctorJob(URLRequest* request);

  static URLRequest::ProtocolFactory Factory;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();

 private:
  ~URLRequestMockLinkDoctorJob() {}
};

# endif  // CHROME_BROWSER_NET_URL_REQUEST_MOCK_LINK_DOCTOR_JOB_H_
