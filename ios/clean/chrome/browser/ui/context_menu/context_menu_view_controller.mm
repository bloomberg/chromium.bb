// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Typedef the block parameter for UIAlertAction for readability.
typedef void (^AlertActionHandler)(UIAlertAction*);
}

@interface ContextMenuViewController ()
// Object to which command messages will be sent.
@property(nonatomic, weak) id dispatcher;
@end

@implementation ContextMenuViewController
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithDispatcher:(id)dispatcher {
  self =
      [[self class] alertControllerWithTitle:nil
                                     message:nil
                              preferredStyle:UIAlertControllerStyleActionSheet];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - ContextMenuConsumer

- (void)setContextMenuTitle:(NSString*)title {
  self.title = title;
}

- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items {
  for (ContextMenuItem* item in items) {
    // Create a block that sends the invocation passed in with the item's
    // configuration to the dispatcher.
    AlertActionHandler handler = ^(UIAlertAction* action) {
      [item.command invokeWithTarget:self.dispatcher];
    };
    [self addAction:[UIAlertAction actionWithTitle:item.title
                                             style:UIAlertActionStyleDefault
                                           handler:handler]];
  }

  // Always add a cancel action.
  [self addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                           style:UIAlertActionStyleCancel
                                         handler:nil]];
}

@end
