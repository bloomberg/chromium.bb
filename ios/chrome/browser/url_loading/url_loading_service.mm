// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadingService::UrlLoadingService(UrlLoadingNotifier* notifier)
    : notifier_(notifier) {}

void UrlLoadingService::SetDelegate(id<URLLoadingServiceDelegate> delegate) {
  delegate_ = delegate;
}

void UrlLoadingService::SetBrowser(Browser* browser) {
  browser_ = browser;
}

void UrlLoadingService::LoadUrlInCurrentTab(
    const ChromeLoadParams& chrome_params) {
  URLLoadResult result = LoadURL(chrome_params, browser_, notifier_);
  switch (result) {
    case URLLoadResult::SWITCH_TO_TAB: {
      SwitchToTab(chrome_params.web_params);
      break;
    }
    case URLLoadResult::DISALLOWED_IN_INCOGNITO: {
      OpenNewTabCommand* command =
          [[OpenNewTabCommand alloc] initWithURL:chrome_params.web_params.url
                                        referrer:web::Referrer()
                                     inIncognito:NO
                                    inBackground:NO
                                        appendTo:kCurrentTab];
      OpenUrlInNewTab(command);
      break;
    }
    case URLLoadResult::INDUCED_CRASH:
    case URLLoadResult::LOADED_PRERENDER:
    case URLLoadResult::RELOADED:
    case URLLoadResult::NORMAL_LOAD:
      // Page load was handled, so nothing else to do.
      break;
  }
}

void UrlLoadingService::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  DCHECK(delegate_);
  // TODO(crbug.com/907527): chip at BVC::switchToTabWithParams by moving some
  // of it here.
  [delegate_ switchToTabWithParams:web_params];
}

void UrlLoadingService::OpenUrlInNewTab(OpenNewTabCommand* command) {
  DCHECK(delegate_);
  DCHECK(browser_);
  ios::ChromeBrowserState* browser_state = browser_->GetBrowserState();

  notifier_->NewTabWillOpenUrl(command.URL, command.inIncognito);

  if (command.inIncognito != browser_state->IsOffTheRecord()) {
    // When sending an open command that switches modes, ensure the tab
    // ends up appended to the end of the model, not just next to what is
    // currently selected in the other mode. This is done with the |appendTo|
    // parameter.
    command.appendTo = kLastTab;
    [delegate_ openURLInNewTabWithCommand:command];
    return;
  }

  TabModel* tab_model = browser_->GetTabModel();

  Tab* adjacent_tab = nil;
  if (command.appendTo == kCurrentTab)
    adjacent_tab = tab_model.currentTab;

  GURL captured_url = command.URL;
  web::Referrer captured_referrer = command.referrer;
  auto openTab = ^{
    [tab_model insertTabWithURL:captured_url
                       referrer:captured_referrer
                     transition:ui::PAGE_TRANSITION_LINK
                         opener:adjacent_tab
                    openedByDOM:NO
                        atIndex:TabModelConstants::kTabPositionAutomatically
                   inBackground:command.inBackground];

    notifier_->NewTabDidOpenUrl(captured_url, command.inIncognito);
  };

  if (!command.inBackground) {
    openTab();
  } else {
    [delegate_ animateOpenBackgroundTabFromCommand:command completion:openTab];
  }
}
