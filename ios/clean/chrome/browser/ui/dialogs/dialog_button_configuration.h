// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_CONFIGURATION_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class DialogConfigurationIdentifier;
enum class DialogButtonStyle : char;

// An object encapsulating the data necessary to set up a dialog button.
@interface DialogButtonConfiguration : NSObject

// Factory method for item creation.  |text| must be non-empty.
+ (nonnull instancetype)configWithText:(nonnull NSString*)text
                                 style:(DialogButtonStyle)style;

// DialogTextFieldConfigurations should be created through the factory method.
- (nonnull instancetype)init NS_UNAVAILABLE;

// The default text to display in the text field, if any.
@property(nonatomic, readonly, copy, nonnull) NSString* text;

// The placehodler text to display in the text field, if any.
@property(nonatomic, readonly) DialogButtonStyle style;

// Unique identifier for this DialogButtonConfiguration.
@property(nonatomic, readonly, strong, nonnull)
    DialogConfigurationIdentifier* identifier;

@end
#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_CONFIGURATION_H_
