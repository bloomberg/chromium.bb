// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"

@implementation BasePasswordsContentViewController
@synthesize delegate = delegate_;

- (instancetype)initWithDelegate:
    (id<BasePasswordsContentViewDelegate>)delegate {
  if (self = [super initWithNibName:nil bundle:nil]) {
    delegate_ = delegate;
  }
  return self;
}

- (NSButton*)addButton:(NSString*)title
                toView:(NSView*)view
                target:(id)target
                action:(SEL)action {
  NSButton* button = DialogButton(title);
  [button setTarget:target];
  [button setAction:action];
  [view addSubview:button];
  return button;
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

- (NSButton*)defaultButton {
  return nil;
}

@end
