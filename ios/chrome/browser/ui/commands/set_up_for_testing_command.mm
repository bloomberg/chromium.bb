// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/set_up_for_testing_command.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "net/base/mac/url_conversions.h"
#import "third_party/google_toolbox_for_mac/src/Foundation/GTMNSDictionary+URLArguments.h"
#include "url/gurl.h"

namespace {
// Clear all browsing data.
NSString* const kClearBrowsingData = @"clearBrowsingData";
// Close existing tabs.
NSString* const kCloseTabs = @"closeTabs";
// Number of new tabs.
NSString* const kNumberOfNewTabs = @"numberOfNewTabs";
}  // namespace

@implementation SetUpForTestingCommand {
  // Whether the browsing data should be cleared.
  BOOL _clearBrowsingData;
  // Whether the existing tabs should be closed.
  BOOL _closeTabs;
  // The number of new tabs to create.
  NSInteger _numberOfNewTabs;
}

@synthesize clearBrowsingData = _clearBrowsingData;
@synthesize closeTabs = _closeTabs;
@synthesize numberOfNewTabs = _numberOfNewTabs;

- (instancetype)initWithURL:(const GURL&)url {
  // TODO(ios): Rewrite to use GetValueForKeyInQuery.
  NSURL* nsurl = net::NSURLWithGURL(url);
  DCHECK(nsurl);
  NSDictionary* urlArguments =
      [NSDictionary gtm_dictionaryWithHttpArgumentsString:nsurl.query];
  BOOL clearBrowsingData =
      [urlArguments objectForKey:kClearBrowsingData] != nil;
  BOOL closeTabs = [urlArguments objectForKey:kCloseTabs] != nil;
  NSInteger numberOfNewTabs =
      [[urlArguments objectForKey:kNumberOfNewTabs] integerValue];

  return [self initWithClearBrowsingData:clearBrowsingData
                               closeTabs:closeTabs
                         numberOfNewTabs:numberOfNewTabs];
}

- (instancetype)initWithClearBrowsingData:(BOOL)clearBrowsingData
                                closeTabs:(BOOL)closeTabs
                          numberOfNewTabs:(NSInteger)numberOfNewTabs {
  self = [super init];
  if (self) {
    _clearBrowsingData = clearBrowsingData;
    _closeTabs = closeTabs;
    _numberOfNewTabs = numberOfNewTabs;
  }
  return self;
}

- (NSInteger)tag {
  return IDC_SETUP_FOR_TESTING;
}

@end
