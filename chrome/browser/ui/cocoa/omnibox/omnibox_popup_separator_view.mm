// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_separator_view.h"

#include "grit/theme_resources.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/resource/resource_bundle.h"

@implementation OmniboxPopupTopSeparatorView

+ (CGFloat)preferredHeight {
  return 1;
}

- (void)drawRect:(NSRect)rect {
  NSRect separatorRect = [self bounds];
  separatorRect.size.height = [self cr_lineWidth];
  [[self strokeColor] set];
  NSRectFillUsingOperation(separatorRect, NSCompositeSourceOver);
}

@end

@implementation OmniboxPopupBottomSeparatorView

+ (CGFloat)preferredHeight {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* shadowImage =
      rb.GetNativeImageNamed(IDR_OVERLAY_DROP_SHADOW).ToNSImage();
  return [shadowImage size].height;
}

- (void)drawRect:(NSRect)rect {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSRect bounds = [self bounds];

  // Draw the shadow.
  NSImage* shadowImage =
      rb.GetNativeImageNamed(IDR_OVERLAY_DROP_SHADOW).ToNSImage();
  [shadowImage drawInRect:bounds
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:1.0];
}

@end
