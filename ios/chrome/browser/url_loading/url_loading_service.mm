// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#include "base/strings/string_number_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "net/base/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Helper method for inducing intentional freezes and crashes, in a separate
// function so it will show up in stack traces.
// If a delay parameter is present, the main thread will be frozen for that
// number of seconds.
// If a crash parameter is "true" (which is the default value), the browser will
// crash after this delay. Any other value will not trigger a crash.
void InduceBrowserCrash(const GURL& url) {
  int delay = 0;
  std::string delay_string;
  if (net::GetValueForKeyInQuery(url, "delay", &delay_string)) {
    base::StringToInt(delay_string, &delay);
  }
  if (delay > 0) {
    sleep(delay);
  }

  bool crash = true;
  std::string crash_string;
  if (net::GetValueForKeyInQuery(url, "crash", &crash_string)) {
    crash = crash_string == "" || crash_string == "true";
  }

  if (crash) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
    // Call another function, so that the above CHECK can't be tail-call
    // optimized. This ensures that this method's name will show up in the stack
    // for easier identification.
    CHECK(true);
  }
}
}

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

void UrlLoadingService::SetAppService(AppUrlLoadingService* app_service) {
  app_service_ = app_service;
}

void UrlLoadingService::SetDelegate(id<URLLoadingServiceDelegate> delegate) {
  delegate_ = delegate;
}

void UrlLoadingService::SetBrowser(Browser* browser) {
  browser_ = browser;
}

void UrlLoadingService::LoadUrlInCurrentTab(
    const ChromeLoadParams& chrome_params) {
  web::NavigationManager::WebLoadParams params = chrome_params.web_params;
  if (chrome_params.disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    SwitchToTab(chrome_params.web_params);
    return;
  }

  ios::ChromeBrowserState* browser_state = browser_->GetBrowserState();

  notifier_->TabWillOpenUrl(params.url, params.transition_type);

  // NOTE: This check for the Crash Host URL is here to avoid the URL from
  // ending up in the history causing the app to crash at every subsequent
  // restart.
  if (params.url.host() == kChromeUIBrowserCrashHost) {
    InduceBrowserCrash(params.url);
    // Under a debugger, the app can continue working even after the CHECK.
    // Adding a return avoids adding the crash url to history.
    notifier_->TabFailedToOpenUrl(params.url, params.transition_type);
    return;
  }

  // Ask the prerender service to load this URL if it can, and return if it does
  // so.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browser_state);
  WebStateList* web_state_list = browser_->GetWebStateList();
  id<SessionWindowRestoring> restorer =
      (id<SessionWindowRestoring>)browser_->GetTabModel();
  if (prerenderService &&
      prerenderService->MaybeLoadPrerenderedURL(
          params.url, params.transition_type, web_state_list, restorer)) {
    notifier_->TabDidPrerenderUrl(params.url, params.transition_type);
    return;
  }

  // Some URLs are not allowed while in incognito.  If we are in incognito and
  // load a disallowed URL, instead create a new tab not in the incognito state.
  if (browser_state->IsOffTheRecord() && !IsURLAllowedInIncognito(params.url)) {
    notifier_->TabFailedToOpenUrl(params.url, params.transition_type);
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:chrome_params.web_params.url
                                      referrer:web::Referrer()
                                   inIncognito:NO
                                  inBackground:NO
                                      appendTo:kCurrentTab];
    OpenUrlInNewTab(command);
    return;
  }

  web::WebState* current_web_state = web_state_list->GetActiveWebState();
  DCHECK(current_web_state);

  BOOL typedOrGeneratedTransition =
      PageTransitionCoreTypeIs(params.transition_type,
                               ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(params.transition_type,
                               ui::PAGE_TRANSITION_GENERATED);
  if (typedOrGeneratedTransition) {
    LoadTimingTabHelper::FromWebState(current_web_state)->DidInitiatePageLoad();
  }

  // If this is a reload initiated from the omnibox.
  // TODO(crbug.com/730192): Add DCHECK to verify that whenever urlToLoad is the
  // same as the old url, the transition type is ui::PAGE_TRANSITION_RELOAD.
  if (PageTransitionCoreTypeIs(params.transition_type,
                               ui::PAGE_TRANSITION_RELOAD)) {
    current_web_state->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
    notifier_->TabDidReloadUrl(params.url, params.transition_type);
    return;
  }

  current_web_state->GetNavigationManager()->LoadURLWithParams(params);

  notifier_->TabDidOpenUrl(params.url, params.transition_type);
}

void UrlLoadingService::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  DCHECK(app_service_);

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
      app_service_->LoadUrlInNewTab(new_tab_command);
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

void UrlLoadingService::OpenUrlInNewTab(OpenNewTabCommand* command) {
  DCHECK(app_service_);
  DCHECK(delegate_);
  DCHECK(browser_);
  ios::ChromeBrowserState* browser_state = browser_->GetBrowserState();

  if (command.inIncognito != browser_state->IsOffTheRecord()) {
    // When sending an open command that switches modes, ensure the tab
    // ends up appended to the end of the model, not just next to what is
    // currently selected in the other mode. This is done with the |appendTo|
    // parameter.
    command.appendTo = kLastTab;
    app_service_->LoadUrlInNewTab(command);
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
  GURL captured_virtual_url = command.virtualURL;
  web::Referrer captured_referrer = command.referrer;
  auto openTab = ^{
    web::NavigationManager::WebLoadParams params(captured_url);
    params.referrer = captured_referrer;
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.virtual_url = captured_virtual_url;
    [tab_model
        insertTabWithLoadParams:params
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
