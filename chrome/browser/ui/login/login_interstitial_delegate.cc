// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_interstitial_delegate.h"

LoginInterstitialDelegate::LoginInterstitialDelegate(
    content::WebContents* web_contents,
    const GURL& request_url,
    base::Closure& callback)
    : callback_(callback) {
  // The interstitial page owns us.
  content::InterstitialPage* interstitial_page =
      content::InterstitialPage::Create(web_contents,
                                        true,
                                        request_url,
                                        this);
  interstitial_page->Show();
}

LoginInterstitialDelegate::~LoginInterstitialDelegate() {
}

void LoginInterstitialDelegate::CommandReceived(const std::string& command) {
  callback_.Run();
}

std::string LoginInterstitialDelegate::GetHTMLContents() {
  // Showing an interstitial results in a new navigation, and a new navigation
  // closes all modal dialogs on the page. Therefore the login prompt must be
  // shown after the interstitial is displayed. This is done by sending a
  // command from the interstitial page as soon as it is loaded.
  return std::string(
      "<!DOCTYPE html>"
      "<html><body><script>"
      "window.domAutomationController.setAutomationId(1);"
      "window.domAutomationController.send('1');"
      "</script></body></html>");
}
