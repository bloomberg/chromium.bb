// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import <objc/runtime.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

namespace {

// Verifies the preconditions for creating a WKWebView. Must be called before
// a WKWebView is allocated. Not verifying the preconditions before creating
// a WKWebView will lead to undefined behavior.
void VerifyWKWebViewCreationPreConditions(
    BrowserState* browser_state,
    WKWebViewConfiguration* configuration) {
  DCHECK(browser_state);
  DCHECK(configuration);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  DCHECK_EQ([config_provider.GetWebViewConfiguration() processPool],
            [configuration processPool]);
}

}  // namespace

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type,
                          id<CRWContextMenuDelegate> context_menu_delegate) {
  VerifyWKWebViewCreationPreConditions(browser_state, configuration);

  GetWebClient()->PreWebViewCreation();
  WKWebView* web_view =
      [[WKWebView alloc] initWithFrame:frame configuration:configuration];

  if (user_agent_type != web::UserAgentType::NONE) {
    web_view.customUserAgent = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(user_agent_type));
  }

  // By default the web view uses a very sluggish scroll speed. Set it to a more
  // reasonable value.
  web_view.scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;

  // Starting in iOS10, |allowsLinkPreview| defaults to YES.  This should be
  // disabled since the default implementation will open the link in Safari.
  // TODO(crbug.com/622746): Remove once web// link preview implementation is
  // created.
  web_view.allowsLinkPreview = NO;

  if (context_menu_delegate) {
    base::scoped_nsobject<CRWContextMenuController> context_menu_controller(
        [[CRWContextMenuController alloc]
               initWithWebView:web_view
            injectionEvaluator:nil
                      delegate:context_menu_delegate]);
    objc_setAssociatedObject(web_view, context_menu_controller.get(),
                             context_menu_controller.get(),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }

  return [web_view autorelease];
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type) {
  return BuildWKWebView(frame, configuration, browser_state, user_agent_type,
                        nil);
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state) {
  return BuildWKWebView(frame, configuration, browser_state,
                        UserAgentType::MOBILE);
}

}  // namespace web
