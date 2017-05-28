// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_view_controller.h"

#include "base/logging.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Typedef the block parameter for UIAlertAction for readability.
typedef void (^AlertActionHandler)(UIAlertAction*);
// Sends |commands| to |dispatcher| using |context| as the menu context.
void DispatchContextMenuCommands(const std::vector<SEL>& commands,
                                 id dispatcher,
                                 ContextMenuContext* context) {
  DCHECK(dispatcher);
  DCHECK(context);
  for (SEL command : commands) {
    IMP command_imp = [dispatcher methodForSelector:command];
    DCHECK(command_imp);
    command_imp(dispatcher, command, context);
  }
}
}

@interface ContextMenuViewController ()
// The dispatcher passed on initialization.
@property(nonatomic, readonly, weak) id<ContextMenuCommands> dispatcher;
// The context passed on initialization.
@property(nonatomic, strong) ContextMenuContext* context;

// Creates an UIAlertAction for |item| using |style| to manage the action's
// appearance.
- (UIAlertAction*)alertActionForItem:(ContextMenuItem*)item
                               style:(UIAlertActionStyle)style;

@end

@implementation ContextMenuViewController
@synthesize dispatcher = _dispatcher;
@synthesize context = _context;

- (instancetype)initWithDispatcher:(id<ContextMenuCommands>)dispatcher {
  self =
      [[self class] alertControllerWithTitle:nil
                                     message:nil
                              preferredStyle:UIAlertControllerStyleActionSheet];
  if (self) {
    DCHECK(dispatcher);
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - ContextMenuConsumer

- (void)setContextMenuContext:(ContextMenuContext*)context {
  DCHECK(context);
  self.context = context;
}

- (void)setContextMenuTitle:(NSString*)title {
  self.title = title;
}

- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items
                 cancelItem:(ContextMenuItem*)cancelItem {
  // Add an alert action for each item in |items|, then add |cancelItem|.
  for (ContextMenuItem* item in items) {
    [self addAction:[self alertActionForItem:item
                                       style:UIAlertActionStyleDefault]];
  }
  [self addAction:[self alertActionForItem:cancelItem
                                     style:UIAlertActionStyleCancel]];
}

#pragma mark -

- (UIAlertAction*)alertActionForItem:(ContextMenuItem*)item
                               style:(UIAlertActionStyle)style {
  DCHECK(item);
  // Create a block that dispatches |item|'s ContextMenuCommands.
  AlertActionHandler handler = ^(UIAlertAction* action) {
    DispatchContextMenuCommands(item.commands, self.dispatcher, self.context);
  };
  return [UIAlertAction actionWithTitle:item.title style:style handler:handler];
}

@end
