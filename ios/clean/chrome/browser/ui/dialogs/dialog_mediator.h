// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"

@protocol DialogConsumer;

// Class responsible for setting up a DialogConsumer.  This class is meant to be
// subclassed, and subclasses are expected to implement the interface in the
// DialogMediator+Subclassing category.
@interface DialogMediator : NSObject<DialogDismissalCommands>

// Supplies UI information to |consumer|.
- (void)updateConsumer:(id<DialogConsumer>)consumer;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_H_
