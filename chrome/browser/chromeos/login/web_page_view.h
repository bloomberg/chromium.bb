// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_VIEW_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

class Profile;

namespace content {
class BrowserContext;
class SiteInstance;
class WebContentsDelegate;
}

namespace views {
class Label;
class Throbber;
}

namespace chromeos {

// WebPageDomView is the view that is rendering the page.
class WebPageDomView : public views::WebView {
 public:
  explicit WebPageDomView(content::BrowserContext* browser_context);

  // Set delegate that will be notified about tab contents changes.
  void SetWebContentsDelegate(content::WebContentsDelegate* delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebPageDomView);
};

// WebPageView represents the view that holds WebPageDomView with
// page rendered in it. While page is loaded spinner overlay is shown.
class WebPageView : public views::View {
 public:
  WebPageView();
  virtual ~WebPageView();

  // Initialize view layout.
  void Init();

  // Initialize the WebView, creating the contents. This should be
  // called once the view has been added to a container.
  void InitWebView(content::SiteInstance* site_instance);

  // Loads the given URL into the page.
  // You must have previously called Init() and SetWebContentsDelegate.
  void LoadURL(const GURL& url);

  // Sets delegate for tab contents changes.
  void SetWebContentsDelegate(content::WebContentsDelegate* delegate);

  // Stops throbber and shows page content (starts renderer_timer_ for that).
  void ShowPageContent();

 protected:
  virtual WebPageDomView* dom_view() = 0;

 private:
  // Overriden from views::View:
  virtual void Layout() OVERRIDE;

  // Called by stop_timer_. Shows rendered page.
  void ShowRenderedPage();

  // Called by start_timer_. Shows throbber and waiting label.
  void ShowWaitingControls();

  // Throbber shown during page load.
  views::Throbber* throbber_;

  // "Connecting..." label shown while waiting for the page to load/render.
  views::Label* connecting_label_;

  // Timer used when waiting for network response.
  base::OneShotTimer<WebPageView> start_timer_;

  // Timer used before toggling loaded page visibility.
  base::OneShotTimer<WebPageView> stop_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebPageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_VIEW_H_
