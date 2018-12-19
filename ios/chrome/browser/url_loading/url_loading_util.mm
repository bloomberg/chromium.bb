// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/tab_restore_service_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void LoadJavaScriptURL(const GURL& url,
                       ios::ChromeBrowserState* browserState,
                       web::WebState* webState) {
  DCHECK(url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(webState);
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browserState);
  if (prerenderService) {
    prerenderService->CancelPrerender();
  }
  NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
      stringByRemovingPercentEncoding];
  if (webState)
    webState->ExecuteUserJavaScript(jsToEval);
}

void RestoreTab(const SessionID sessionID,
                WindowOpenDisposition disposition,
                ios::ChromeBrowserState* browserState) {
  TabRestoreServiceDelegateImplIOS* delegate =
      TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(browserState);
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          browserState->GetOriginalChromeBrowserState());
  restoreService->RestoreEntryById(delegate, sessionID, disposition);
}
