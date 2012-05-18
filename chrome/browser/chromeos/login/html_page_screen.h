// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"

namespace chromeos {

class ViewScreenDelegate;

class HTMLPageView : public WebPageView {
 public:
  explicit HTMLPageView(content::BrowserContext* browser_context);

 protected:
  virtual WebPageDomView* dom_view() OVERRIDE;

 private:
  // View that renders page.
  WebPageDomView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(HTMLPageView);
};

// HTMLPageScreen is used to show arbitrary HTML page. It is used to show
// simple screens like recover.
class HTMLPageScreen : public ViewScreen<HTMLPageView>,
                       public WebPageScreen {
 public:
  HTMLPageScreen(ViewScreenDelegate* delegate, const std::string& url);
  virtual ~HTMLPageScreen();

 protected:
  // Overrides WebPageScreen:
  virtual void OnNetworkTimeout() OVERRIDE;

 private:
  // ViewScreen implementation:
  virtual void CreateView() OVERRIDE;
  virtual void Refresh() OVERRIDE;
  virtual HTMLPageView* AllocateView() OVERRIDE;

  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code) OVERRIDE;

  // URL to navigate.
  std::string url_;

  UnhandledKeyboardEventHandler unhandled_keyboard_handler_;

  DISALLOW_COPY_AND_ASSIGN(HTMLPageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
