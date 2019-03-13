// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_PARAMS_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_PARAMS_H_

#import "ios/chrome/browser/ui/chrome_load_params.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/window_open_disposition.h"

// UrlLoadingService wrapper around web::NavigationManager::WebLoadParams,
// WindowOpenDisposition and parameters from OpenNewTabCommand.
// This is used when a URL is opened.
struct UrlLoadParams {
 public:
  // Initializes a UrlLoadParams intended to open in current page.
  static UrlLoadParams* InCurrentTab(
      const web::NavigationManager::WebLoadParams& web_params);
  static UrlLoadParams* InCurrentTab(const GURL& url);

  // Initializes a UrlLoadParams intended to open in a new page.
  static UrlLoadParams* InNewTab(const GURL& url,
                                 const GURL& virtual_url,
                                 const web::Referrer& referrer,
                                 bool in_incognito,
                                 bool in_background,
                                 OpenPosition append_to);

  // Initializes a UrlLoadParams intended to open a new page.
  static UrlLoadParams* InNewEmptyTab(bool in_incognito, bool in_background);

  // Initializes a UrlLoadParams intended to open a URL from browser chrome
  // (e.g., settings). This will always open in a new foreground tab in
  // non-incognito mode.
  static UrlLoadParams* InNewFromChromeTab(const GURL& url);

  // Initializes a UrlLoadParams with |in_incognito| and an |origin_point|.
  static UrlLoadParams* InNewForegroundTab(bool in_incognito,
                                           CGPoint origin_point);

  // Initializes a UrlLoadParams with |in_incognito| and an |origin_point| of
  // CGPointZero.
  static UrlLoadParams* InNewForegroundTab(bool in_incognito);

  // Initializes a UrlLoadParams intended to switch to tab.
  static UrlLoadParams* SwitchToTab(
      const web::NavigationManager::WebLoadParams& web_params);

  // Allow copying UrlLoadParams.
  UrlLoadParams(const UrlLoadParams& other);
  UrlLoadParams& operator=(const UrlLoadParams& other);

  // The wrapped params.
  web::NavigationManager::WebLoadParams web_params;

  // The disposition of the URL being opened.
  WindowOpenDisposition disposition;

  // Parameters for when opening in new tab:

  // Whether this requests opening in incognito or not.
  bool in_incognito;

  // Location where the new tab should be opened.
  OpenPosition append_to;

  // Origin point of the action triggering this command, in main window
  // coordinates.
  CGPoint origin_point;

  // Whether or not this URL command comes from a chrome context (e.g.,
  // settings), as opposed to a web page context.
  bool from_chrome;

  // Whether the new tab command was initiated by the user (e.g. by tapping the
  // new tab button in the tools menu) or not (e.g. opening a new tab via a
  // Javascript action). Defaults to |true|. Only used when the |web_params.url|
  // isn't valid.
  bool user_initiated;

  // Whether the new tab command should also trigger the omnibox to be focused.
  // Only used when the |web_params.url| isn't valid.
  bool should_focus_omnibox;

  bool in_background() {
    return disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

 private:
  UrlLoadParams();
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_PARAMS_H_
