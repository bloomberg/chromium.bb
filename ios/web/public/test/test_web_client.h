// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
#define WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/web_client.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace web {

// A WebClient used for testing purposes.
class TestWebClient : public web::WebClient {
 public:
  TestWebClient();
  ~TestWebClient() override;

  // WebClient implementation.
  void AddAdditionalSchemes(std::vector<url::SchemeWithType>*) const override;
  // Returns true for kTestWebUIScheme URL scheme.
  bool IsAppSpecificURL(const GURL& url) const override;
  NSString* GetEarlyPageScript() const override;
  void AllowCertificateError(WebState*,
                             int cert_error,
                             const net::SSLInfo&,
                             const GURL&,
                             bool overridable,
                             const base::Callback<void(bool)>&) override;

  // Changes Early Page Script for testing purposes.
  void SetEarlyPageScript(NSString* page_script);

  // Accessors for last arguments passed to AllowCertificateError.
  int last_cert_error_code() const { return last_cert_error_code_; };
  const net::SSLInfo& last_cert_error_ssl_info() const {
    return last_cert_error_ssl_info_;
  }
  const GURL& last_cert_error_request_url() const {
    return last_cert_error_request_url_;
  }
  bool last_cert_error_overridable() { return last_cert_error_overridable_; }

 private:
  base::scoped_nsobject<NSString> early_page_script_;
  // Last arguments passed to AllowCertificateError.
  int last_cert_error_code_;
  net::SSLInfo last_cert_error_ssl_info_;
  GURL last_cert_error_request_url_;
  bool last_cert_error_overridable_;
};

}  // namespace web

#endif // WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
