// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/interstitials/security_interstitial_page.h"

class GURL;

namespace content{
class WebContents;
}

// This class is responsible for showing/hiding the interstitial page that is
// shown when a captive portal triggers an SSL error.
// It deletes itself when the interstitial page is closed.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService, which can only be accessed on the
// UI thread. Only used when ENABLE_CAPTIVE_PORTAL_DETECTION is true.
class CaptivePortalBlockingPage : public SecurityInterstitialPage {
 public:
  // Interstitial type, for testing.
  static const void* kTypeForTesting;

  CaptivePortalBlockingPage(content::WebContents* web_contents,
                            const GURL& request_url,
                            const base::Callback<void(bool)>& callback);
  ~CaptivePortalBlockingPage() override;

  // SecurityInterstitialPage method:
  const void* GetTypeForTesting() const override;

 protected:
  // SecurityInterstitialPage methods:
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  bool ShouldCreateNewNavigation() const override;

  // InterstitialPageDelegate method:
  void CommandReceived(const std::string& command) override;

 private:
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
