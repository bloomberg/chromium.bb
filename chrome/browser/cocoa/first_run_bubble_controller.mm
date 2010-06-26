// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/first_run_bubble_controller.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/cocoa/l10n_util.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#include "chrome/browser/search_engines/util.h"
#include "grit/generated_resources.h"

@interface FirstRunBubbleController(Private)
- (id)initRelativeToView:(NSView*)view
                  offset:(NSPoint)offset
                 profile:(Profile*)profile;
@end

@implementation FirstRunBubbleController

+ (FirstRunBubbleController*) showForView:(NSView*)view
                                   offset:(NSPoint)offset
                                  profile:(Profile*)profile {
  // Autoreleases itself on bubble close.
  return [[FirstRunBubbleController alloc] initRelativeToView:view
                                                       offset:offset
                                                      profile:profile];
}

- (id)initRelativeToView:(NSView*)view
                  offset:(NSPoint)offset
                 profile:(Profile*)profile {
  if ((self = [super initWithWindowNibPath:@"FirstRunBubble"
                            relativeToView:view
                                    offset:offset])) {
    profile_ = profile;
    [self showWindow:nil];
  }
  return self;
}

- (void)awakeFromNib {
  [[self bubble] setBubbleType:info_bubble::kWhiteInfoBubble];

  DCHECK(header_);
  [header_ setStringValue:cocoa_l10n_util::ReplaceNSStringPlaceholders(
      [header_ stringValue], GetDefaultSearchEngineName(profile_), NULL)];

  // Adapt window size to bottom buttons. Do this before all other layouting.
  CGFloat dy = cocoa_l10n_util::VerticallyReflowGroup([[self bubble] subviews]);
  NSSize ds = NSMakeSize(0, dy);
  ds = [[self bubble] convertSize:ds toView:nil];

  NSRect frame = [[self window] frame];
  frame.origin.y -= ds.height;
  frame.size.height += ds.height;
  [[self window] setFrame:frame display:YES];
}

@end  // FirstRunBubbleController
