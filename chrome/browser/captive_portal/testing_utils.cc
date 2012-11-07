// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/testing_utils.h"

#include "net/http/http_util.h"

namespace captive_portal {

scoped_refptr<net::HttpResponseHeaders> CreateResponseHeaders(
    const std::string& response_headers) {
  std::string raw_headers =
      net::HttpUtil::AssembleRawHeaders(response_headers.c_str(),
                                        response_headers.length());
  return new net::HttpResponseHeaders(raw_headers);
}

CaptivePortalDetectorTestBase::CaptivePortalDetectorTestBase()
    : detector_(NULL) {
}

CaptivePortalDetectorTestBase::~CaptivePortalDetectorTestBase() {
}

void CaptivePortalDetectorTestBase::SetTime(const base::Time& time) {
  detector()->set_time_for_testing(time);
}

void CaptivePortalDetectorTestBase::AdvanceTime(const base::TimeDelta& delta) {
  detector()->advance_time_for_testing(delta);
}

bool CaptivePortalDetectorTestBase::FetchingURL() {
  return detector()->FetchingURL();
}

void CaptivePortalDetectorTestBase::OnURLFetchComplete(
    net::URLFetcher* fetcher) {
  detector()->OnURLFetchComplete(fetcher);
}

}  // namespace captive_portal
