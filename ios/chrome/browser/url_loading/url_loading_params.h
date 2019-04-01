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
  static UrlLoadParams* InCurrentTab(const GURL& url, const GURL& virtual_url);

  // Initializes a UrlLoadParams intended to open in a new page.
  static UrlLoadParams* InNewTab(
      const web::NavigationManager::WebLoadParams& web_params);
  static UrlLoadParams* InNewTab(const GURL& url);
  static UrlLoadParams* InNewTab(const GURL& url, const GURL& virtual_url);

  // Initializes a UrlLoadParams intended to switch to tab.
  static UrlLoadParams* SwitchToTab(
      const web::NavigationManager::WebLoadParams& web_params);

  // Set individual parameters, builder style.
  UrlLoadParams* Transition(ui::PageTransition transition_type);
  UrlLoadParams* InIncognito(bool in_incognito);
  UrlLoadParams* Referrer(const web::Referrer& referrer);
  UrlLoadParams* InBackground(bool in_background);
  UrlLoadParams* AppendTo(OpenPosition append_to);
  UrlLoadParams* OriginPoint(CGPoint origin_point);
  UrlLoadParams* FromChrome(bool from_chrome);
  UrlLoadParams* UserInitiated(bool user_initiated);
  UrlLoadParams* ShouldFocusOmnibox(bool should_focus_omnibox);
  UrlLoadParams* Disposition(WindowOpenDisposition disposition);

  // Allow copying UrlLoadParams.
  UrlLoadParams(const UrlLoadParams& other);
  UrlLoadParams& operator=(const UrlLoadParams& other);

  // The wrapped params.
  web::NavigationManager::WebLoadParams web_params;

  // The disposition of the URL being opened. Defaults to
  // |WindowOpenDisposition::NEW_FOREGROUND_TAB|.
  WindowOpenDisposition disposition;

  // Parameters for when opening in new tab:

  // Whether this requests opening in incognito or not. Defaults to |false|.
  bool in_incognito;

  // Location where the new tab should be opened. Defaults to |kLastTab|.
  OpenPosition append_to;

  // Origin point of the action triggering this command, in main window
  // coordinates. Defaults to |CGPointZero|.
  CGPoint origin_point;

  // Whether or not this URL command comes from a chrome context (e.g.,
  // settings), as opposed to a web page context. Defaults to |false|.
  bool from_chrome;

  // Whether the new tab command was initiated by the user (e.g. by tapping the
  // new tab button in the tools menu) or not (e.g. opening a new tab via a
  // Javascript action). Defaults to |true|. Only used when the |web_params.url|
  // isn't valid.
  bool user_initiated;

  // Whether the new tab command should also trigger the omnibox to be focused.
  // Only used when the |web_params.url| isn't valid. Defaults to |false|.
  bool should_focus_omnibox;

  bool in_background() {
    return disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

 private:
  UrlLoadParams();
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_PARAMS_H_
