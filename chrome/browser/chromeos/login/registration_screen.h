// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"

class GURL;
class Profile;

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

namespace chromeos {

class ViewScreenDelegate;

// Class that renders host registration page.
class RegistrationDomView : public WebPageDomView {
 public:
  RegistrationDomView() {}

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RegistrationDomView);
};

// Class that displays screen contents: page and throbber while waiting.
class RegistrationView : public WebPageView {
 public:
  RegistrationView() : dom_view_(new RegistrationDomView()) {}

 protected:
  virtual WebPageDomView* dom_view() OVERRIDE;

 private:
  // View that renders page.
  RegistrationDomView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationView);
};

// RegistrationScreen represents screen that is shown during OOBE.
// It renders host page served from resources that includes iframe with
// registration page specified in the startup customization manifest.
// Partner registration page notifies host page on registration result.
// Host page notifies that back to RegistrationScreen.
class RegistrationScreen : public ViewScreen<RegistrationView>,
                           public WebPageScreen,
                           public WebPageDelegate {
 public:
  explicit RegistrationScreen(ViewScreenDelegate* delegate);

  // WebPageDelegate implementation:
  virtual void OnPageLoaded() OVERRIDE;
  virtual void OnPageLoadFailed(const std::string& url) OVERRIDE;

  // Handler factory for net::URLRequestFilter::AddHostnameHandler.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  // ViewScreen implementation:
  virtual void CreateView() OVERRIDE;
  virtual void Refresh() OVERRIDE;
  virtual RegistrationView* AllocateView() OVERRIDE;

  // TabContentsDelegate implementation:
  // Deprecated. Please use two-argument variant.
  // TODO(adriansc): Remove this method once refactoring changed all call sites.
  virtual TabContents* OpenURLFromTab(
      TabContents* source,
      const GURL& url,
      const GURL& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition) OVERRIDE;
  virtual TabContents* OpenURLFromTab(TabContents* source,
                                      const OpenURLParams& params) OVERRIDE;

  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RegistrationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
