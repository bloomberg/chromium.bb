// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONSUMER_H_

#import <Foundation/Foundation.h>

@class DialogButtonConfiguration;
@class DialogTextFieldConfiguration;

// A DialogConsumer uses data provided by this protocol to configure and run a
// dialog.  A DialogConsumer can be configured multiple times before
// presentation, but once the consumer is presented, the appearance of the
// dialog will be immutable.
@protocol DialogConsumer<NSObject>

// Sets the title of the dialog.
- (void)setDialogTitle:(nullable NSString*)title;

// Sets the message of the dialog.
- (void)setDialogMessage:(nullable NSString*)message;

// Sets the button items to display in the dialog.
- (void)setDialogButtonConfigurations:
    (nullable NSArray<DialogButtonConfiguration*>*)buttonConfigs;

// Sets the text field items to display in the dialog.
- (void)setDialogTextFieldConfigurations:
    (nullable NSArray<DialogTextFieldConfiguration*>*)textFieldConfigs;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_CONSUMER_H_
