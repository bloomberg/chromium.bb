// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_close_button.h"

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/animation_utils.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMKeyValueAnimation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace  {
const CGFloat kFramesPerSecond = 16; // Determined experimentally to look good.
const CGFloat kCloseAnimationDuration = 0.1;

// Images that are used for all close buttons. Set up in +initialize.
NSImage* gHoverNoneImage = nil;
NSImage* gHoverMouseOverImage = nil;
NSImage* gHoverMouseDownImage = nil;

// Strings that are used for all close buttons. Set up in +initialize.
NSString* gTooltip = nil;
NSString* gDescription = nil;

// If this string is changed, the setter (currently setFadeOutValue:) must
// be changed as well to match.
NSString* const kFadeOutValueKeyPath = @"fadeOutValue";
}  // namespace

@interface HoverCloseButton ()

// Common initialization routine called from initWithFrame and awakeFromNib.
- (void)commonInit;

// Called by |fadeOutAnimation_| when animated value changes.
- (void)setFadeOutValue:(CGFloat)value;

@end

@implementation HoverCloseButton

+ (void)initialize {
  if ([self class] == [HoverCloseButton class]) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    gHoverNoneImage = bundle.GetNativeImageNamed(IDR_TAB_CLOSE).ToNSImage();
    gHoverMouseOverImage =
        bundle.GetNativeImageNamed(IDR_TAB_CLOSE_H).ToNSImage();
    gHoverMouseDownImage =
        bundle.GetNativeImageNamed(IDR_TAB_CLOSE_P).ToNSImage();

    // Grab some strings that are used by all close buttons.
    gDescription = [l10n_util::GetNSStringWithFixup(IDS_ACCNAME_CLOSE) copy];
    gTooltip = [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_CLOSE_TAB) copy];
  }
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self commonInit];
}

- (void)removeFromSuperview {
  // -stopAnimation will call the animationDidStop: delegate method
  // which will release our animation.
  [fadeOutAnimation_ stopAnimation];
  [super removeFromSuperview];
}

- (void)animationDidStop:(NSAnimation*)animation {
  DCHECK(animation == fadeOutAnimation_);
  [fadeOutAnimation_ setDelegate:nil];
  [fadeOutAnimation_ release];
  fadeOutAnimation_ = nil;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];
}

- (void)drawRect:(NSRect)dirtyRect {
  // Close boxes align left horizontally, and align center vertically.
  // http:crbug.com/14739 requires this.
  NSRect imageRect = NSZeroRect;
  imageRect.size = [gHoverMouseOverImage size];

  NSRect destRect = [self bounds];
  destRect.origin.y = floor((NSHeight(destRect) / 2)
                            - (NSHeight(imageRect) / 2));
  destRect.size = imageRect.size;

  switch(self.hoverState) {
    case kHoverStateMouseOver:
      [gHoverMouseOverImage drawInRect:destRect
                              fromRect:imageRect
                             operation:NSCompositeSourceOver
                              fraction:1.0
                        respectFlipped:YES
                                 hints:nil];
      break;

    case kHoverStateMouseDown:
      [gHoverMouseDownImage drawInRect:destRect
                              fromRect:imageRect
                             operation:NSCompositeSourceOver
                              fraction:1.0
                        respectFlipped:YES
                                 hints:nil];
      break;

    default:
    case kHoverStateNone: {
      CGFloat value = 1.0;
      if (fadeOutAnimation_) {
        value = [fadeOutAnimation_ currentValue];
        NSImage* previousImage = nil;
        if (previousState_ == kHoverStateMouseOver) {
          previousImage = gHoverMouseOverImage;
        } else {
          previousImage =  gHoverMouseDownImage;
        }
        [previousImage drawInRect:destRect
                         fromRect:imageRect
                        operation:NSCompositeSourceOver
                         fraction:1.0 - value
                   respectFlipped:YES
                            hints:nil];
      }
      [gHoverNoneImage drawInRect:destRect
                         fromRect:imageRect
                        operation:NSCompositeSourceOver
                         fraction:value
                   respectFlipped:YES
                            hints:nil];
      break;
    }
  }
}

- (void)setFadeOutValue:(CGFloat)value {
  [self setNeedsDisplay];
}

- (void)setHoverState:(HoverState)state {
  if (state != self.hoverState) {
    previousState_ = self.hoverState;
    [super setHoverState:state];
    // Only animate the HoverStateNone case.
    if (state == kHoverStateNone) {
      DCHECK(fadeOutAnimation_ == nil);
      fadeOutAnimation_ =
          [[GTMKeyValueAnimation alloc] initWithTarget:self
                                               keyPath:kFadeOutValueKeyPath];
      [fadeOutAnimation_ setDuration:kCloseAnimationDuration];
      [fadeOutAnimation_ setFrameRate:kFramesPerSecond];
      [fadeOutAnimation_ setDelegate:self];
      [fadeOutAnimation_ startAnimation];
    } else {
      // -stopAnimation will call the animationDidStop: delegate method
      // which will clean up the animation.
      [fadeOutAnimation_ stopAnimation];
    }
  }
}

- (void)commonInit {
  // Set accessibility description.
  NSCell* cell = [self cell];
  [cell accessibilitySetOverrideValue:gDescription
                         forAttribute:NSAccessibilityDescriptionAttribute];

  // Add a tooltip. Using 'owner:self' means that
  // -view:stringForToolTip:point:userData: will be called to provide the
  // tooltip contents immediately before showing it.
  [self addToolTipRect:[self bounds] owner:self userData:NULL];

  // Initialize previousState.
  previousState_ = kHoverStateNone;
}

// Called each time a tooltip is about to be shown.
- (NSString*)view:(NSView*)view
 stringForToolTip:(NSToolTipTag)tag
            point:(NSPoint)point
         userData:(void*)userData {
  if (self.hoverState == kHoverStateMouseOver) {
    // In some cases (e.g. the download tray), the button is still in the
    // hover state, but is outside the bounds of its parent and not visible.
    // Don't show the tooltip in that case.
    NSRect buttonRect = [self frame];
    NSRect parentRect = [[self superview] bounds];
    if (NSIntersectsRect(buttonRect, parentRect))
      return gTooltip;
  }

  return nil;  // Do not show the tooltip.
}

@end
