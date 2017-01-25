// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_app_delegate.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web_view/public/criwv.h"
#import "ios/web_view/shell/shell_delegate.h"
#import "ios/web_view/shell/shell_view_controller.h"

@interface ShellAppDelegate () {
  base::scoped_nsobject<ShellDelegate> _delegate;
}
@end

@implementation ShellAppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];

  _delegate.reset([[ShellDelegate alloc] init]);
  [CRIWV configureWithDelegate:_delegate];

  base::scoped_nsobject<ShellViewController> controller(
      [[ShellViewController alloc] init]);
  self.window.rootViewController = controller;
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
  [CRIWV shutDown];
}

@end
