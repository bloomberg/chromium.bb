// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_DELAYED_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_DELAYED_RESPONSE_PROVIDER_H_

#include <memory>

#import "ios/web/public/test/response_providers/response_provider.h"

namespace web {

// A Response provider that delays the response provided by another reponse
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

  // Creates a GCDWebServerResponse that will proxy the object returned by
  // Forwards to |delayed_provider_->GetGCDWebServerResponse(request)|.
  // The read operation will be delayed by |delay_| seconds.
  GCDWebServerResponse* GetGCDWebServerResponse(
      const Request& request) override;

 private:
  std::unique_ptr<web::ResponseProvider> delayed_provider_;
  double delay_;

  DISALLOW_COPY_AND_ASSIGN(DelayedResponseProvider);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_DELAYED_RESPONSE_PROVIDER_H_
