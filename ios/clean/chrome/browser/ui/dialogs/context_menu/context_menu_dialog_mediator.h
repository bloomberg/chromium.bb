// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

@protocol ContextMenuCommands;
@protocol ContextMenuDismissalCommands;
@class ContextMenuDialogRequest;

// Class responsible for setting up a DialogConsumer for JavaScript dialogs.
@interface ContextMenuDialogMediator : DialogMediator

// Designated initializer for a mediator that uses |request| to configure
// consumers and |dispatcher| for dismissal.
- (nullable instancetype)initWithRequest:
    (nonnull ContextMenuDialogRequest*)request NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// The dispatcher.
@property(nonatomic, weak, nullable)
    id<ContextMenuCommands, ContextMenuDismissalCommands>
        dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_CONTEXT_MENU_CONTEXT_MENU_DIALOG_MEDIATOR_H_
