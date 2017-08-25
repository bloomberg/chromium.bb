// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_view_controller.h"

#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Helper object that performs no-ops for DialogDismissalCommands.
@interface DummyDialogDismissalDispatcher : NSObject<DialogDismissalCommands>
@end

@implementation DummyDialogDismissalDispatcher
- (void)
dismissDialogWithButtonID:(nonnull DialogConfigurationIdentifier*)buttonID
          textFieldValues:(nonnull DialogTextFieldValues*)textFieldValues {
}
@end

@interface TestDialogViewController ()
@property(nonatomic, readonly, strong)
    DummyDialogDismissalDispatcher* dispatcher;
@end

@implementation TestDialogViewController
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithStyle:(UIAlertControllerStyle)style {
  DummyDialogDismissalDispatcher* dispatcher =
      [[DummyDialogDismissalDispatcher alloc] init];
  if ((self = [super initWithStyle:style dispatcher:dispatcher])) {
    _dispatcher = dispatcher;
  }
  return self;
}

- (instancetype)initWithStyle:(UIAlertControllerStyle)style
                   dispatcher:(id<DialogDismissalCommands>)dispatcher {
  return [self initWithStyle:style];
}

@end
