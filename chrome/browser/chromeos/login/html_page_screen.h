// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"

namespace chromeos {

class ViewScreenDelegate;

class HTMLPageDomView : public WebPageDomView {
 public:
  HTMLPageDomView() {}

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLPageDomView);
};

class HTMLPageView : public WebPageView {
 public:
  HTMLPageView();

 protected:
  virtual WebPageDomView* dom_view() OVERRIDE;

 private:
  // View that renders page.
  HTMLPageDomView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(HTMLPageView);
};

// HTMLPageScreen is used to show arbitrary HTML page. It is used to show
// simple screens like recover.
class HTMLPageScreen : public ViewScreen<HTMLPageView>,
                       public WebPageScreen,
                       public WebPageDelegate {
 public:
  HTMLPageScreen(ViewScreenDelegate* delegate, const std::string& url);
  virtual ~HTMLPageScreen();

  // WebPageDelegate implementation:
  virtual void OnPageLoaded() OVERRIDE;
  virtual void OnPageLoadFailed(const std::string& url) OVERRIDE;

 protected:
  // Overrides WebPageScreen:
  virtual void OnNetworkTimeout() OVERRIDE;

 private:
  // ViewScreen implementation:
  virtual void CreateView() OVERRIDE;
  virtual void Refresh() OVERRIDE;
  virtual HTMLPageView* AllocateView() OVERRIDE;

  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code) OVERRIDE;

  // URL to navigate.
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(HTMLPageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HTML_PAGE_SCREEN_H_
