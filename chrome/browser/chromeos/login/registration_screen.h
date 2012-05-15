// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"

class GURL;
class Profile;

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

namespace chromeos {

class ViewScreenDelegate;

// Class that displays screen contents: page and throbber while waiting.
class RegistrationView : public WebPageView {
 public:
  explicit RegistrationView(content::BrowserContext* browser_context);

 protected:
  virtual WebPageDomView* dom_view() OVERRIDE;

 private:
  // View that renders page.
  WebPageDomView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationView);
};

// RegistrationScreen represents screen that is shown during OOBE.
// It renders host page served from resources that includes iframe with
// registration page specified in the startup customization manifest.
// Partner registration page notifies host page on registration result.
// Host page notifies that back to RegistrationScreen.
class RegistrationScreen : public ViewScreen<RegistrationView>,
                           public WebPageScreen {
 public:
  explicit RegistrationScreen(ViewScreenDelegate* delegate);
  virtual ~RegistrationScreen();

  // Handler factory for net::URLRequestFilter::AddHostnameHandler.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  // ViewScreen implementation:
  virtual void CreateView() OVERRIDE;
  virtual void Refresh() OVERRIDE;
  virtual RegistrationView* AllocateView() OVERRIDE;

  // content::WebContentsDelegate implementation:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code) OVERRIDE;

  UnhandledKeyboardEventHandler unhandled_keyboard_handler_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_REGISTRATION_SCREEN_H_
