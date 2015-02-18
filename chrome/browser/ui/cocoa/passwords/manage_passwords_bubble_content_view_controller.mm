// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"

#include "base/mac/scoped_nsobject.h"

@implementation ManagePasswordsBubbleContentViewController

- (NSButton*)addButton:(NSString*)title
                toView:(NSView*)view
                target:(id)target
                action:(SEL)action {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setTitle:title];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSSmallControlSize];
  [button setTarget:target];
  [button setAction:action];
  [button sizeToFit];
  [view addSubview:button.get()];
  return button.autorelease();
}

- (NSTextField*)addTitleLabel:(NSString*)title toView:(NSView*)view {
  base::scoped_nsobject<NSTextField> label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setDrawsBackground:NO];
  [label setBezeled:NO];
  [label setStringValue:title];
  [label sizeToFit];
  [view addSubview:label.get()];
  return label.autorelease();
}

- (NSTextField*)addLabel:(NSString*)title toView:(NSView*)view {
  NSTextField* label = [self addTitleLabel:title toView:view];
  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [label setFont:font];
  [[label cell] setWraps:YES];
  return label;
}

- (void)bubbleWillDisappear {
}

@end
