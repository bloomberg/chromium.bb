// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_VIEW_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/timer.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/view.h"

class Profile;
class TabContentsDelegate;

namespace views {
class Label;
class Throbber;
}  // namespace views

namespace chromeos {

// Delegate interface for listening to common events during page load.
class WebPageDelegate {
 public:
  virtual ~WebPageDelegate() {}

  // Notify about document load event.
  virtual void OnPageLoaded() = 0;

  // Notify about navigation errors.
  virtual void OnPageLoadFailed(const std::string& url) = 0;
};

// Base class for tab contents for pages rendered on wizard screens.
class WizardWebPageViewTabContents : public TabContents {
 public:
  WizardWebPageViewTabContents(Profile* profile,
                               SiteInstance* site_instance,
                               WebPageDelegate* page_delegate);

  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      bool is_main_frame,
      int error_code,
      const GURL& url,
      bool showing_repost_interstitial);

  virtual void DidDisplayInsecureContent();
  virtual void DidRunInsecureContent(const std::string& security_origin);
  virtual void DocumentLoadedInFrame(long long frame_id);
  virtual void DidFinishLoad(long long frame_id);

 private:
  WebPageDelegate* page_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WizardWebPageViewTabContents);
};

// WebPageDomView is the view that is rendering the page.
class WebPageDomView : public DOMView {
 public:
  WebPageDomView() : page_delegate_(NULL) {}

  // Set delegate that will be notified about tab contents changes.
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

  // Set delegate that will be notified about page events.
  void set_web_page_delegate(WebPageDelegate* delegate) {
    page_delegate_ = delegate;
  }

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance) = 0;
  WebPageDelegate* page_delegate_;

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

  // Initialize the DOM view, creating the contents. This should be
  // called once the view has been added to a container.
  void InitDOM(Profile* profile, SiteInstance* site_instance);

  // Loads the given URL into the page.
  // You must have previously called Init() and SetTabContentsDelegate.
  void LoadURL(const GURL& url);

  // Sets delegate for tab contents changes.
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

  // Set delegate that will be notified about page events.
  void SetWebPageDelegate(WebPageDelegate* delegate);

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
