// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/steps/launch_to_foreground.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/clean/chrome/app/application_state.h"
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
                      openURL:[NSURL URLWithString:@"chrome://newtab"]
                      options:@{}];
  }
  [super motionEnded:motion withEvent:event];
}

@end
