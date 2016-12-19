// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/chrome/app/steps/launch_to_foreground.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/app/application_state.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/web_capabilities.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Temporary class to trigger URL-opening events from a shake gesture.
@interface ShakeCatchingWindow : UIWindow
@end

@implementation BeginForegrounding

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_BACKGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  GetApplicationContext()->OnAppEnterForeground();
}

@end

@implementation BrowserStateInitializer

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.browserState && state.phase == APPLICATION_BACKGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  DCHECK(!state.browserState->IsOffTheRecord());
  [self setInitialCookiesPolicy:state.browserState];
}

// Copied verbatim from MainController.
- (void)setInitialCookiesPolicy:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  net::CookieStoreIOS::CookiePolicy policy = net::CookieStoreIOS::BLOCK;

  auto settingsFactory =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
  DCHECK(settingsFactory);
  ContentSetting cookieSetting = settingsFactory->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, nullptr);

  if (!web::IsAcceptCookieControlSupported()) {
    // Override the Accept Cookie policy as ALLOW is the only policy
    // supported by //web.
    policy = net::CookieStoreIOS::ALLOW;
    if (cookieSetting == CONTENT_SETTING_BLOCK) {
      settingsFactory->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                CONTENT_SETTING_ALLOW);
    }
  } else {
    switch (cookieSetting) {
      case CONTENT_SETTING_ALLOW:
        policy = net::CookieStoreIOS::ALLOW;
        break;
      case CONTENT_SETTING_BLOCK:
        policy = net::CookieStoreIOS::BLOCK;
        break;
      default:
        NOTREACHED() << "Unsupported cookie policy.";
        break;
    }
  }

  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&net::CookieStoreIOS::SetCookiePolicy, policy));
}

@end

@implementation PrepareForUI

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_BACKGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  state.window =
      [[ShakeCatchingWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  [state.window makeKeyWindow];
}

@end

@implementation CompleteForegrounding
- (BOOL)canRunInState:(ApplicationState*)state {
  return state.window.keyWindow && state.phase == APPLICATION_BACKGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  state.phase = APPLICATION_FOREGROUNDED;
}

@end

@implementation ShakeCatchingWindow

#pragma mark - UIResponder

- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent*)event {
  if (motion == UIEventSubtypeMotionShake) {
    UIApplication* app = [UIApplication sharedApplication];
    [app.delegate application:app
                      openURL:[NSURL URLWithString:@"https://www.google.com"]
                      options:@{}];
  }
  [super motionEnded:motion withEvent:event];
}

@end
