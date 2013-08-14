// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_view.h"

#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "ui/base/cocoa/nsgraphics_context_additions.h"

@implementation ToolbarView

@synthesize dividerOpacity = dividerOpacity_;

// Prevent mouse down events from moving the parent window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)drawRect:(NSRect)rect {
  NSPoint position = [[self window]
      themeImagePositionForAlignment:THEME_IMAGE_ALIGN_WITH_TAB_STRIP];
  [[NSGraphicsContext currentContext] cr_setPatternPhase:position forView:self];
  [self drawBackgroundWithOpaque:YES];
}

// Override of |-[BackgroundGradientView strokeColor]|; make it respect opacity.
- (NSColor*)strokeColor {
  return [[super strokeColor] colorWithAlphaComponent:[self dividerOpacity]];
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
    return NSAccessibilityToolbarRole;

  return [super accessibilityAttributeValue:attribute];
}

- (ViewID)viewID {
  return VIEW_ID_TOOLBAR;
}

- (BOOL)isOpaque {
  return YES;
}

@end
