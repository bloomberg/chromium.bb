// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

#include "base/callback.h"
#include "ios/chrome/browser/interstitials/ios_security_interstitial_page.h"
#include "url/gurl.h"

@protocol IOSCaptivePortalBlockingPageDelegate;

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error is caused by the user being behind a captive
// portal.
// It deletes itself when the interstitial page is closed.
class IOSCaptivePortalBlockingPage : public IOSSecurityInterstitialPage {
 public:
  ~IOSCaptivePortalBlockingPage() override;

  // Creates a captive portal blocking page. If the blocking page isn't shown,
  // the caller is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. The |web_state| and |request_url|
  // of the request which this interstitial page is associated. |landing_url| is
  // the web page which allows the user to complete their connection to the
  // network. This blocking page will not take ownership of |delegate|, so it
  // must externally outlive this blocking page. |callback| will be called after
  // the user is done interacting with this interstitial.
  IOSCaptivePortalBlockingPage(
      web::WebState* web_state,
      const GURL& request_url,
      const GURL& landing_url,
      id<IOSCaptivePortalBlockingPageDelegate> delegate,
      const base::Callback<void(bool)>& callback);

 private:
  // IOSSecurityInterstitialPage overrides:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(base::DictionaryValue*) const override;
  void AfterShow() override;
  void OnDontProceed() override;
  void CommandReceived(const std::string& command) override;

  // The landing page url for the captive portal network.
  const GURL landing_url_;

  __weak id<IOSCaptivePortalBlockingPageDelegate> delegate_;

  // |callback_| is run when the user is done with this interstitial. The
  // parameter will be true if the user has successfully connected to the
  // captive portal network.
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(IOSCaptivePortalBlockingPage);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
