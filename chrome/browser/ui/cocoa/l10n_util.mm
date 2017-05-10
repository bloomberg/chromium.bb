// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/l10n_util.h"

#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_features.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace cocoa_l10n_util {

NSInteger CompareFrameY(id view1, id view2, void* context) {
  CGFloat y1 = NSMinY([view1 frame]);
  CGFloat y2 = NSMinY([view2 frame]);
  if (y1 < y2)
    return NSOrderedAscending;
  else if (y1 > y2)
    return NSOrderedDescending;
  else
    return NSOrderedSame;
}

NSSize WrapOrSizeToFit(NSView* view) {
  if ([view isKindOfClass:[NSTextField class]]) {
    NSTextField* textField = static_cast<NSTextField*>(view);
    if ([textField isEditable])
      return NSZeroSize;
    CGFloat heightChange =
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
    return NSMakeSize(0.0, heightChange);
  }
  if ([view isKindOfClass:[NSMatrix class]]) {
    NSMatrix* radioGroup = static_cast<NSMatrix*>(view);
    [GTMUILocalizerAndLayoutTweaker wrapRadioGroupForWidth:radioGroup];
    return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
  }
  if ([view isKindOfClass:[NSButton class]]) {
    NSButton* button = static_cast<NSButton*>(view);
    NSButtonCell* buttonCell = [button cell];
    // Decide it's a checkbox via showsStateBy and highlightsBy.
    if (([buttonCell showsStateBy] == NSCellState) &&
        ([buttonCell highlightsBy] == NSCellState)) {
      [GTMUILocalizerAndLayoutTweaker wrapButtonTitleForWidth:button];
      return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
    }
  }
  return [GTMUILocalizerAndLayoutTweaker sizeToFitView:view];
}

CGFloat VerticallyReflowGroup(NSArray* views) {
  views = [views sortedArrayUsingFunction:CompareFrameY
                                  context:NULL];
  CGFloat localVerticalShift = 0;
  for (NSInteger index = [views count] - 1; index >= 0; --index) {
    NSView* view = [views objectAtIndex:index];

    NSSize delta = WrapOrSizeToFit(view);
    localVerticalShift += delta.height;
    if (localVerticalShift) {
      NSPoint origin = [view frame].origin;
      origin.y -= localVerticalShift;
      [view setFrameOrigin:origin];
    }
  }
  return localVerticalShift;
}

NSString* ReplaceNSStringPlaceholders(NSString* formatString,
                                      const base::string16& a,
                                      size_t* offset) {
  return base::SysUTF16ToNSString(base::ReplaceStringPlaceholders(
      base::SysNSStringToUTF16(formatString), a, offset));
}

NSString* TooltipForURLAndTitle(NSString* url, NSString* title) {
  if ([title length] == 0)
    return url;
  else if ([url length] == 0 || [url isEqualToString:title])
    return title;
  else
    return [NSString stringWithFormat:@"%@\n%@", title, url];
}

bool ShouldDoExperimentalRTLLayout() {
  return base::i18n::IsRTL() && base::FeatureList::IsEnabled(features::kMacRTL);
}

bool ShouldFlipWindowControlsInRTL() {
  return ShouldDoExperimentalRTLLayout() && base::mac::IsAtLeastOS10_12();
}

// TODO(lgrey): Remove these when all builds are on 10.12 SDK.
#if defined(MAC_OS_X_VERSION_10_12) && \
    (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12)
#warning LeadingCellImagePosition/TrailingCellImagePosition \
  should be removed since the deployment target is >= 10.12
#endif

#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
NSCellImagePosition LeadingCellImagePosition() {
  return ShouldDoExperimentalRTLLayout() ? NSImageRight : NSImageLeft;
}
NSCellImagePosition TrailingCellImagePosition() {
  return ShouldDoExperimentalRTLLayout() ? NSImageLeft : NSImageRight;
}
#else
NSCellImagePosition LeadingCellImagePosition() {
  return NSImageLeading;
}
NSCellImagePosition TrailingCellImagePosition() {
  return NSImageTrailing;
}
#endif  // MAC_OS_X_VERSION_10_12

// Adapted from Apple's RTL docs (goo.gl/cBaFnT)
NSImage* FlippedImage(NSImage* image) {
  const NSSize size = [image size];
  NSImage* flipped_image = [[[NSImage alloc] initWithSize:size] autorelease];

  [flipped_image lockFocus];
  [[NSGraphicsContext currentContext]
      setImageInterpolation:NSImageInterpolationHigh];

  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform translateXBy:size.width yBy:0];
  [transform scaleXBy:-1 yBy:1];
  [transform concat];

  [image drawAtPoint:NSZeroPoint
            fromRect:NSMakeRect(0, 0, size.width, size.height)
           operation:NSCompositeSourceOver
            fraction:1.0];

  [flipped_image unlockFocus];

  return flipped_image;
}

}  // namespace cocoa_l10n_util
