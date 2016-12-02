// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_native_content.h"

#include "base/mac/scoped_nsobject.h"

@implementation TestNativeContent {
  GURL _URL;
  GURL _virtualURL;
  base::scoped_nsobject<UIView> _view;
}
- (instancetype)initWithURL:(const GURL&)URL
                 virtualURL:(const GURL&)virtualURL {
  self = [super init];
  if (self) {
    _URL = URL;
    _virtualURL = virtualURL;
  }
  return self;
}

- (BOOL)respondsToSelector:(SEL)selector {
  if (selector == @selector(virtualURL)) {
    return _virtualURL.is_valid();
  }
  return [super respondsToSelector:selector];
}

- (NSString*)title {
  return @"Test Title";
}

- (const GURL&)url {
  return _URL;
}

- (GURL)virtualURL {
  return _virtualURL;
}

- (UIView*)view {
  return nil;
}

- (void)handleLowMemory {
}

- (BOOL)isViewAlive {
  return YES;
}

- (void)reload {
}
@end
