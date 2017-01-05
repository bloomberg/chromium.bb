// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"

namespace content {
class WebContents;
}

// Provides embedder-specific logic for the security error page controller.
class ChromeControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  ChromeControllerClient(
      content::WebContents* web_contents,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper);
  ~ChromeControllerClient() override;

  // security_interstitials::ControllerClient overrides
  bool CanLaunchDateAndTimeSettings() override;
  void LaunchDateAndTimeSettings() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeControllerClient);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_
