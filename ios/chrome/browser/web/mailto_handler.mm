// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/mailto_handler.h"

#import "base/strings/sys_string_conversions.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MailtoHandler
@synthesize appName = _appName;
@synthesize appStoreID = _appStoreID;

- (instancetype)initWithName:(NSString*)appName
                  appStoreID:(NSString*)appStoreID {
  self = [super init];
  if (self) {
    _appName = [appName copy];
    _appStoreID = [appStoreID copy];
  }
  return self;
}

- (BOOL)isAvailable {
  NSURL* baseURL = [NSURL URLWithString:[self beginningScheme]];
  NSURL* testURL = [NSURL
      URLWithString:[NSString stringWithFormat:@"%@://", [baseURL scheme]]];
  return [[UIApplication sharedApplication] canOpenURL:testURL];
}

- (NSString*)beginningScheme {
  // Subclasses should override this method.
  return @"mailtohandler:/co?";
}

- (NSSet<NSString*>*)supportedHeaders {
  return [NSSet<NSString*>
      setWithObjects:@"to", @"cc", @"bcc", @"subject", @"body", nil];
}

- (NSString*)rewriteMailtoURL:(const GURL&)gURL {
  if (!gURL.SchemeIs(url::kMailToScheme))
    return nil;
  NSMutableArray* outParams = [NSMutableArray array];
  NSString* recipient = base::SysUTF8ToNSString(gURL.path());
  if ([recipient length]) {
    [outParams addObject:[NSString stringWithFormat:@"to=%@", recipient]];
  }
  NSString* query = base::SysUTF8ToNSString(gURL.query());
  for (NSString* keyvalue : [query componentsSeparatedByString:@"&"]) {
    NSArray* pair = [keyvalue componentsSeparatedByString:@"="];
    if ([pair count] != 2U || ![[self supportedHeaders] containsObject:pair[0]])
      continue;
    [outParams
        addObject:[NSString stringWithFormat:@"%@=%@", pair[0], pair[1]]];
  }
  return [NSString stringWithFormat:@"%@%@", [self beginningScheme],
                                    [outParams componentsJoinedByString:@"&"]];
}

@end
