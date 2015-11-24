// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "components/security_interstitials/core/controller_client.h"

namespace content {
class WebContents;
}

// Provides embedder-specific logic for the security error page controller.
class ChromeControllerClient : public security_interstitials::ControllerClient {
 public:
  explicit ChromeControllerClient(content::WebContents* web_contents);
  ~ChromeControllerClient() override;

 protected:
  // security_interstitials::ControllerClient overrides
  void OpenUrlInCurrentTab(const GURL& url) override;
  const std::string& GetApplicationLocale() override;
  PrefService* GetPrefService() override;
  const std::string GetExtendedReportingPrefName() override;

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeControllerClient);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_CHROME_CONTROLLER_CLIENT_H_
