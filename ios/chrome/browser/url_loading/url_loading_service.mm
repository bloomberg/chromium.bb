// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UrlLoadingServiceUrlLoader : NSObject <UrlLoader>
- (instancetype)initWithUrlLoadingService:(UrlLoadingService*)service;
@end

@implementation UrlLoadingServiceUrlLoader {
  UrlLoadingService* service_;
}

- (instancetype)initWithUrlLoadingService:(UrlLoadingService*)service {
  DCHECK(service);
  if (self = [super init]) {
    service_ = service;
  }
  return self;
}

- (void)loadURLWithParams:(const ChromeLoadParams&)chromeParams {
  service_->LoadUrlInCurrentTab(chromeParams);
}

- (void)webPageOrderedOpen:(OpenNewTabCommand*)command {
  service_->OpenUrlInNewTab(command);
}

@end

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

  const GURL& url = web_params.url;

  WebStateList* web_state_list = browser_->GetWebStateList();
  NSInteger new_web_state_index =
      web_state_list->GetIndexOfInactiveWebStateWithURL(url);
  bool old_tab_is_ntp_without_history =
      IsNTPWithoutHistory(web_state_list->GetActiveWebState());

  if (new_web_state_index == WebStateList::kInvalidIndex) {
    // If the tab containing the URL has been closed.
    if (old_tab_is_ntp_without_history) {
      // It is NTP, just load the URL.
      ChromeLoadParams currentPageParams(web_params);
      LoadUrlInCurrentTab(currentPageParams);
    } else {
      // Open the URL in foreground.
      ios::ChromeBrowserState* browser_state = browser_->GetBrowserState();
      OpenNewTabCommand* new_tab_command =
          [[OpenNewTabCommand alloc] initWithURL:url
                                        referrer:web::Referrer()
                                     inIncognito:browser_state->IsOffTheRecord()
                                    inBackground:NO
                                        appendTo:kCurrentTab];
      [delegate_ openURLInNewTabWithCommand:new_tab_command];
    }
    return;
  }

  notifier_->WillSwitchToTabWithUrl(url, new_web_state_index);

  NSInteger old_web_state_index = web_state_list->active_index();
  web_state_list->ActivateWebStateAt(new_web_state_index);

  // Close the tab if it is NTP with no back/forward history to avoid having
  // empty tabs.
  if (old_tab_is_ntp_without_history) {
    web_state_list->CloseWebStateAt(old_web_state_index,
                                    WebStateList::CLOSE_USER_ACTION);
  }

  notifier_->DidSwitchToTabWithUrl(url, new_web_state_index);
}

// TODO(crbug.com/907527): migrate the extra work done in main_controller when
// [delegate_ openURLInNewTabWithCommand:] is called, and remove
// openURLInNewTabWithCommand from delegate.
void UrlLoadingService::OpenUrlInNewTab(OpenNewTabCommand* command) {
  DCHECK(delegate_);
  DCHECK(browser_);
  ios::ChromeBrowserState* browser_state = browser_->GetBrowserState();

  if (command.inIncognito != browser_state->IsOffTheRecord()) {
    // When sending an open command that switches modes, ensure the tab
    // ends up appended to the end of the model, not just next to what is
    // currently selected in the other mode. This is done with the |appendTo|
    // parameter.
    command.appendTo = kLastTab;
    [delegate_ openURLInNewTabWithCommand:command];
    return;
  }

  // Notify only after checking incognito match, otherwise the delegate will
  // take of changing the mode and try again. Notifying before the checks can
  // lead to be calling it twice, and calling 'did' below once.
  notifier_->NewTabWillOpenUrl(command.URL, command.inIncognito);

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

id<UrlLoader> UrlLoadingService::GetUrlLoader() {
  if (!url_loader_) {
    url_loader_ =
        [[UrlLoadingServiceUrlLoader alloc] initWithUrlLoadingService:this];
  }
  return url_loader_;
}
