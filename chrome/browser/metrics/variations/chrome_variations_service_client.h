// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_

#include "base/basictypes.h"
#include "components/variations/variations_service_client.h"

// ChromeVariationsServiceClient provides an implementation of
// VariationsServiceClient that depends on chrome/.
class ChromeVariationsServiceClient
    : public chrome_variations::VariationsServiceClient {
 public:
  ChromeVariationsServiceClient();
  ~ChromeVariationsServiceClient() override;

  // chrome_variations::VariationsServiceClient:
  std::string GetApplicationLocale() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeVariationsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
