// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/preview_drop_shadow_view.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

@implementation PreviewDropShadowView

+ (CGFloat)preferredHeight {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* shadowImage =
      rb.GetNativeImageNamed(IDR_PREVIEW_DROP_SHADOW).ToNSImage();
  return [shadowImage size].height;
}

- (void)drawRect:(NSRect)rect {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSRect bounds = [self bounds];

  // Draw the shadow.
  NSImage* shadowImage =
      rb.GetNativeImageNamed(IDR_PREVIEW_DROP_SHADOW).ToNSImage();
  [shadowImage drawInRect:bounds
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:1.0];
}

@end
