// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "crnet_consumer_app_delegate.h"

#import <CrNet/CrNet.h>

#include "base/format_macros.h"
#import "crnet_consumer_view_controller.h"

@implementation CrNetConsumerAppDelegate {
  NSUInteger _counter;
}

@synthesize window;
@synthesize viewController;

// Returns a file name to save net internals logging. This method suffixes
// the ivar |_counter| to the file name so a new name can be obtained by
// modifying that.
- (NSString*)currentNetLogFileName {
  return [NSString
      stringWithFormat:@"crnet-consumer-net-log%" PRIuNS ".json", _counter];
}

- (NSString*)SDCHPrefStoreFileName {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSArray* possibleURLs = [manager
      URLsForDirectory:NSApplicationSupportDirectory
             inDomains:NSUserDomainMask];
  NSURL* appSupportDir = [possibleURLs firstObject];
  if (appSupportDir == nil)
    return nil;
  NSURL* prefStoreFile = [NSURL URLWithString:@"sdch-prefs.json"
                                relativeToURL:appSupportDir];
  NSError* error = nil;
  [manager createDirectoryAtURL:appSupportDir
      withIntermediateDirectories:YES
                       attributes:nil
                            error:&error];
  return error != nil ? [prefStoreFile path] : nil;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [CrNet setPartialUserAgent:@"Dummy/1.0"];
  [CrNet setQuicEnabled:YES];
  [CrNet setSDCHEnabled:YES withPrefStore:[self SDCHPrefStoreFileName]];
  [CrNet install];
  [CrNet startNetLogToFile:[self currentNetLogFileName] logBytes:NO];

  NSURLSessionConfiguration* config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  [CrNet installIntoSessionConfiguration:config];

  // Just for fun, don't route chromium.org requests through CrNet.
  //
  // |chromiumPrefix| is declared outside the scope of the request block so that
  // the block references something outside of its own scope, and cannot be
  // declared as a global block. This makes sure the block is
  // an __NSStackBlock__, and verifies the fix for http://crbug.com/436175 .
  NSString *chromiumPrefix = @"www.chromium.org";
  [CrNet setRequestFilterBlock:^BOOL (NSURLRequest *request) {
      BOOL isChromiumSite = [[[request URL] host] hasPrefix:chromiumPrefix];
      return !isChromiumSite;
  }];

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.viewController =
      [[CrNetConsumerViewController alloc] initWithNibName:nil bundle:nil];
  self.window.rootViewController = self.viewController;
  [self.window makeKeyAndVisible];

  return YES;
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
  [CrNet stopNetLog];
  [CrNet clearCacheWithCompletionCallback:^(int error) {
    NSLog(@"Cache cleared: %d\n", error);
  }];
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
  _counter++;
  [CrNet startNetLogToFile:[self currentNetLogFileName] logBytes:NO];
}

@end
