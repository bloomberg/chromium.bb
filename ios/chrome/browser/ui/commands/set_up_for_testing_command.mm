// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/set_up_for_testing_command.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Clear all browsing data.
const char kClearBrowsingData[] = "clearBrowsingData";
// Close existing tabs.
const char kCloseTabs[] = "closeTabs";
// Number of new tabs.
const char kNumberOfNewTabs[] = "numberOfNewTabs";
}  // namespace

@implementation SetUpForTestingCommand

@synthesize clearBrowsingData = _clearBrowsingData;
@synthesize closeTabs = _closeTabs;
@synthesize numberOfNewTabs = _numberOfNewTabs;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithURL:(const GURL&)url {
  std::string value;
  int numberOfNewTabs = 0;
  if (net::GetValueForKeyInQuery(url, kNumberOfNewTabs, &value)) {
    if (!base::StringToInt(value, &numberOfNewTabs) || numberOfNewTabs < 0) {
      // Ignore incorrect value for "numberOfNewTabs".
      numberOfNewTabs = 0;
    }
  }
  std::string ignored;
  bool clearBrowsingData =
      net::GetValueForKeyInQuery(url, kClearBrowsingData, &ignored);
  bool closeTabs = net::GetValueForKeyInQuery(url, kCloseTabs, &ignored);
  return [self initWithClearBrowsingData:clearBrowsingData
                               closeTabs:closeTabs
                         numberOfNewTabs:numberOfNewTabs];
}

- (instancetype)initWithClearBrowsingData:(BOOL)clearBrowsingData
                                closeTabs:(BOOL)closeTabs
                          numberOfNewTabs:(NSInteger)numberOfNewTabs {
  self = [super initWithTag:IDC_SETUP_FOR_TESTING];
  if (self) {
    _clearBrowsingData = clearBrowsingData;
    _closeTabs = closeTabs;
    _numberOfNewTabs = numberOfNewTabs;
  }
  return self;
}

@end
