// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/tab_restore_service_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

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

bool IsURLAllowedInIncognito(const GURL& url) {
  // Most URLs are allowed in incognito; the following is an exception.
  return !(url.SchemeIs(kChromeUIScheme) && url.host() == kChromeUIHistoryHost);
}

void LoadJavaScriptURL(const GURL& url,
                       ios::ChromeBrowserState* browser_state,
                       web::WebState* web_state) {
  DCHECK(url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(web_state);
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browser_state);
  if (prerenderService) {
    prerenderService->CancelPrerender();
  }
  NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
      stringByRemovingPercentEncoding];
  if (web_state)
    web_state->ExecuteUserJavaScript(jsToEval);
}

void RestoreTab(const SessionID session_id,
                WindowOpenDisposition disposition,
                ios::ChromeBrowserState* browser_state) {
  TabRestoreServiceDelegateImplIOS* delegate =
      TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
          browser_state);
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState());
  restoreService->RestoreEntryById(delegate, session_id, disposition);
}

URLLoadResult LoadURL(const ChromeLoadParams& chrome_params,
                      ios::ChromeBrowserState* browser_state,
                      WebStateList* web_state_list,
                      id<SessionWindowRestoring> restorer) {
  web::NavigationManager::WebLoadParams params = chrome_params.web_params;
  if (chrome_params.disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    return URLLoadResult::SWITCH_TO_TAB;
  }

  web::WebState* current_web_state = web_state_list->GetActiveWebState();
  DCHECK(current_web_state);
  if (params.transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
    bool isExpectingVoiceSearch =
        VoiceSearchNavigationTabHelper::FromWebState(current_web_state)
            ->IsExpectingVoiceSearch();
    new_tab_page_uma::RecordActionFromOmnibox(browser_state, params.url,
                                              params.transition_type,
                                              isExpectingVoiceSearch);
  }

  // NOTE: This check for the Crash Host URL is here to avoid the URL from
  // ending up in the history causing the app to crash at every subsequent
  // restart.
  if (params.url.host() == kChromeUIBrowserCrashHost) {
    InduceBrowserCrash(params.url);
    // Under a debugger, the app can continue working even after the CHECK.
    // Adding a return avoids adding the crash url to history.
    return URLLoadResult::INDUCED_CRASH;
  }

  // Ask the prerender service to load this URL if it can, and return if it does
  // so.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browser_state);
  if (prerenderService &&
      prerenderService->MaybeLoadPrerenderedURL(
          params.url, params.transition_type, web_state_list, restorer)) {
    return URLLoadResult::LOADED_PRERENDER;
  }

  // Some URLs are not allowed while in incognito.  If we are in incognito and
  // load a disallowed URL, instead create a new tab not in the incognito state.
  if (browser_state->IsOffTheRecord() && !IsURLAllowedInIncognito(params.url)) {
    return URLLoadResult::DISALLOWED_IN_INCOGNITO;
  }

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
    return URLLoadResult::RELOADED;
  }

  current_web_state->GetNavigationManager()->LoadURLWithParams(params);

  // Deactivate the NTP immediately on a load to hide the NTP quickly, but after
  // calling -LoadURLWithParams. Otherwise, if the webState has never been
  // visible (such as during startup with an NTP), it's possible the webView can
  // trigger a unnecessary load for chrome://newtab.
  if (params.url.GetOrigin() != kChromeUINewTabURL) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(current_web_state);
    if (NTPHelper && NTPHelper->IsActive()) {
      NTPHelper->Deactivate();
    }
  }

  return URLLoadResult::NORMAL_LOAD;
}
