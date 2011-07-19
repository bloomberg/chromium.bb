// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_view.h"

#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"

@implementation ToolbarView

@synthesize dividerOpacity = dividerOpacity_;

// Prevent mouse down events from moving the parent window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)drawRect:(NSRect)rect {
  // The toolbar's background pattern is phased relative to the
  // tab strip view's background pattern.
  NSPoint phase = [[self window] themePatternPhase];
  [[NSGraphicsContext currentContext] setPatternPhase:phase];
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

// Some toolbar buttons draw differently depending on the fouc state of the
// window.
- (void)windowFocusDidChange:(NSNotification*)notification {
  [self setNeedsDisplay:YES];
}

- (void)viewWillMoveToWindow:(NSWindow*)window {
  if ([self window]) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidBecomeKeyNotification
                object:[self window]];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidBecomeMainNotification
                object:[self window]];
  }
  if (window) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowFocusDidChange:)
               name:NSWindowDidBecomeKeyNotification
             object:[self window]];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowFocusDidChange:)
               name:NSWindowDidBecomeMainNotification
             object:[self window]];
  }
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (BOOL)isOpaque {
  return YES;
}

@end
