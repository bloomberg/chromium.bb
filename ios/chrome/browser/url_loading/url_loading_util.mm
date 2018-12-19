// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_util.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
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
