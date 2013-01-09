// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

ChromeWebContentsHandler::ChromeWebContentsHandler() {
}

ChromeWebContentsHandler::~ChromeWebContentsHandler() {
}

// Opens a new URL inside |source|. |context| is the browser context that the
// browser should be owned by. |params| contains the URL to open and various
// attributes such as disposition. On return |out_new_contents| contains the
// WebContents the URL is opened in. Returns the web contents opened by the
// browser.
WebContents* ChromeWebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    WebContents* source,
    const OpenURLParams& params) {
  if (!context)
    return NULL;

  Profile* profile = Profile::FromBrowserContext(context);

  chrome::HostDesktopType desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  if (source) {
    Browser* source_browser = chrome::FindBrowserWithWebContents(source);
    if (source_browser)
      desktop_type = source_browser->host_desktop_type();
  }

  Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
  const bool browser_created = !browser;
  if (!browser)
    browser = new Browser(
        Browser::CreateParams(Browser::TYPE_TABBED, profile, desktop_type));
  chrome::NavigateParams nav_params(browser, params.url, params.transition);
  nav_params.referrer = params.referrer;
  if (source && source->IsCrashed() &&
      params.disposition == CURRENT_TAB &&
      params.transition == content::PAGE_TRANSITION_LINK) {
    nav_params.disposition = NEW_FOREGROUND_TAB;
  } else {
    nav_params.disposition = params.disposition;
  }
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = true;
  chrome::Navigate(&nav_params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != nav_params.browser))
    browser->window()->Close();

  return nav_params.target_contents;
}

// Creates a new tab with |new_contents|. |context| is the browser context that
// the browser should be owned by. |source| is the WebContent where the
// operation originated. |disposition| controls how the new tab should be
// opened. |initial_pos| is the position of the window if a new window is
// created.  |user_gesture| is true if the operation was started by a user
// gesture.
void ChromeWebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    WebContents* source,
    WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  if (!context)
    return;

  Profile* profile = Profile::FromBrowserContext(context);

  chrome::HostDesktopType desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  if (source) {
    Browser* source_browser = chrome::FindBrowserWithWebContents(source);
    if (source_browser)
      desktop_type = source_browser->host_desktop_type();
  }

  Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
  const bool browser_created = !browser;
  if (!browser)
    browser = new Browser(
        Browser::CreateParams(Browser::TYPE_TABBED, profile, desktop_type));
  chrome::NavigateParams params(browser, new_contents);
  params.source_contents = source;
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  chrome::Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}
