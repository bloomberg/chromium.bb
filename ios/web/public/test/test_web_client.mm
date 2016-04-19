// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/test_web_client.h"

#include "base/logging.h"

namespace web {

TestWebClient::TestWebClient()
    : last_cert_error_code_(0), last_cert_error_overridable_(true) {}

TestWebClient::~TestWebClient() {}

NSString* TestWebClient::GetEarlyPageScript() const {
  return early_page_script_ ? early_page_script_.get() : @"";
}

void TestWebClient::SetEarlyPageScript(NSString* page_script) {
  early_page_script_.reset([page_script copy]);
}

void TestWebClient::AllowCertificateError(
    WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  last_cert_error_code_ = cert_error;
  last_cert_error_ssl_info_ = ssl_info;
  last_cert_error_request_url_ = request_url;
  last_cert_error_overridable_ = overridable;

  callback.Run(false);
}

}  // namespace web
