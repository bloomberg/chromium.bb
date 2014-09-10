// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fast_resize_view.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/cocoa/animation_utils.h"

@implementation FastResizeView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    ScopedCAActionDisabler disabler;
    base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
    [layer setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setLayer:layer];
    [self setWantsLayer:YES];
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

@end

