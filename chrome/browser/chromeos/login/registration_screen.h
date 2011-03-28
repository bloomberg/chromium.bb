// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class GURL;
class Profile;
class SiteContents;
class WizardScreenDelegate;

namespace chromeos {

// Class that renders host registration page.
class RegistrationDomView : public WebPageDomView {
 public:
  RegistrationDomView() {}

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance) {
    return new WizardWebPageViewTabContents(profile,
                                            instance,
                                            page_delegate_);
  }

  DISALLOW_COPY_AND_ASSIGN(RegistrationDomView);
};

// Class that displays screen contents: page and throbber while waiting.
class RegistrationView : public WebPageView {
 public:
  RegistrationView() : dom_view_(new RegistrationDomView()) {}

 protected:
  virtual WebPageDomView* dom_view() { return dom_view_; }

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
  explicit RegistrationScreen(WizardScreenDelegate* delegate);

  // WebPageDelegate implementation:
  virtual void OnPageLoaded();
  virtual void OnPageLoadFailed(const std::string& url);

  // Sets the url for registration host page. Used in tests.
  static void set_registration_host_page_url(const GURL& url);

  // Handler factory for net::URLRequestFilter::AddHostnameHandler.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  // ViewScreen implementation:
  virtual void CreateView();
  virtual void Refresh();
  virtual RegistrationView* AllocateView();

  // TabContentsDelegate implementation:
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code);

  // Url of account creation page. Overriden by tests.
  static scoped_ptr<GURL> host_page_url_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
