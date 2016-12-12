// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/native_content_controller.h"

#import <UIKit/UIKit.h>

#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/mac/objc_property_releaser.h"

@implementation NativeContentController {
  GURL _url;
  base::mac::ObjCPropertyReleaser _propertyReleaser_NativeContentController;
}

@synthesize view = _view;
@synthesize title = _title;
@synthesize url = _url;

- (instancetype)initWithNibName:(NSString*)nibName url:(const GURL&)url {
  self = [super init];
  if (self) {
    _propertyReleaser_NativeContentController.Init(
        self, [NativeContentController class]);
    if (nibName.length) {
      [base::mac::FrameworkBundle() loadNibNamed:nibName
                                           owner:self
                                         options:nil];
    }
    _url = url;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithURL:(const GURL&)url {
  return [self initWithNibName:nil url:url];
}

- (void)dealloc {
  [_view removeFromSuperview];
  [super dealloc];
}

#pragma mark CRWNativeContent

- (void)handleLowMemory {
  // TODO(pinkerton): What should this do? Toss the view?
}

- (BOOL)isViewAlive {
  // TODO(pinkerton): See handleLowMemory above.
  return YES;
}

- (void)reload {
  // Not implemented in base class.
}

@end
