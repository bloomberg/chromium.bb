// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_app_delegate.h"

#import "ios/web_view/public/cwv.h"
#import "ios/web_view/shell/shell_delegate.h"
#import "ios/web_view/shell/shell_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellAppDelegate ()
@property(nonatomic, strong) ShellDelegate* delegate;
@end

@implementation ShellAppDelegate

@synthesize delegate = _delegate;
@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];

  self.delegate = [[ShellDelegate alloc] init];
  [CWV configureWithDelegate:_delegate];

  [self.window makeKeyAndVisible];

  ShellViewController* controller = [[ShellViewController alloc] init];
  self.window.rootViewController = controller;

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
  [CWV shutDown];
}

@end
