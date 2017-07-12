// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/new_tab_command.h"

#import "ios/chrome/browser/ui/commands/ios_command_ids.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NewTabCommand

@synthesize incognito = _incognito;
@synthesize originPoint = _originPoint;

- (instancetype)initWithIncognito:(BOOL)incognito
                      originPoint:(CGPoint)originPoint {
  int tag = incognito ? IDC_NEW_INCOGNITO_TAB : IDC_NEW_TAB;
  if ((self = [super initWithTag:tag])) {
    _incognito = incognito;
    _originPoint = originPoint;
  }
  return self;
}

- (instancetype)initWithIncognito:(BOOL)incognito {
  return [self initWithIncognito:incognito originPoint:CGPointZero];
}

@end
