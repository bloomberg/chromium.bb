// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_

#include <memory>

#import "ios/web/public/test/http_server/response_provider.h"

namespace web {

// A Response provider that delays the response provided by another response
// provider
class DelayedResponseProvider : public ResponseProvider {
 public:
  // Creates a DelayedResponseProvider that delays the response from
  // |delayed_provider| by |delay| seconds.
  DelayedResponseProvider(
      std::unique_ptr<web::ResponseProvider> delayed_provider,
      double delay);
  ~DelayedResponseProvider() override;

  // Forwards to |delayed_provider_|.
  bool CanHandleRequest(const Request& request) override;

  // Creates a test_server::HttpResponse that will delay the read operation
  // by |delay_| seconds.
  std::unique_ptr<net::test_server::HttpResponse> GetEmbeddedTestServerResponse(
      const Request& request) override;

 private:
  std::unique_ptr<web::ResponseProvider> delayed_provider_;
  double delay_;

  DISALLOW_COPY_AND_ASSIGN(DelayedResponseProvider);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_
