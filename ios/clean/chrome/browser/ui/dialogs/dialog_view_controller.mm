// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_configuration_identifier.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Typedef the block parameter for UIAlertAction for readability.
typedef void (^AlertActionHandler)(UIAlertAction*);
}

@interface DialogViewController ()

// The dispatcher used for dismissal;
@property(nonatomic, readonly, strong) id<DialogDismissalCommands>
    dismissalDispatcher;

// Objects provided through the DialogConsumer protocol.
@property(nonatomic, readonly, copy)
    NSArray<DialogButtonConfiguration*>* buttonConfigs;
@property(nonatomic, readonly, copy)
    NSArray<DialogTextFieldConfiguration*>* textFieldConfigs;

// The strings corresponding with the text fields.
@property(nonatomic, readonly)
    NSMutableDictionary<DialogConfigurationIdentifier*, NSString*>*
        textFieldValues;

// Creates an AlertActionHandler that sends a DialogDismissalCommand with |tag|.
- (AlertActionHandler)actionForButtonConfiguration:
    (DialogButtonConfiguration*)buttonConfig;
// Adds buttons for each item in |buttonItems|.
- (void)addButtons;
// Adds text fields for each item in |textFieldItems|.
- (void)addTextFields;

@end

@implementation DialogViewController

@synthesize dismissalDispatcher = _dismissalDispatcher;
@synthesize buttonConfigs = _buttonConfigs;
@synthesize textFieldConfigs = _textFieldConfigs;
@synthesize textFieldValues = _textFieldValues;

- (instancetype)initWithStyle:(UIAlertControllerStyle)style
                   dispatcher:(id<DialogDismissalCommands>)dispatcher {
  self = [[self class] alertControllerWithTitle:nil
                                        message:nil
                                 preferredStyle:style];
  if (self) {
    _dismissalDispatcher = dispatcher;
  }
  return self;
}

#pragma mark - Accessors

- (NSMutableDictionary<DialogConfigurationIdentifier*, NSString*>*)
    textFieldValues {
  // Early return if text field items haven't been supplied or the text fields
  // have not been instantiated.
  NSUInteger textFieldCount = self.textFieldConfigs.count;
  if (!textFieldCount || textFieldCount != self.textFields.count)
    return nil;
  // Lazily create the array and update its contents.
  if (!_textFieldValues) {
    _textFieldValues =
        [[NSMutableDictionary<DialogConfigurationIdentifier*, NSString*> alloc]
            init];
  }
  for (NSUInteger fieldIndex = 0; fieldIndex < textFieldCount; ++fieldIndex) {
    _textFieldValues[self.textFieldConfigs[fieldIndex].identifier] =
        self.textFields[fieldIndex].text;
  }
  return _textFieldValues;
}

#pragma mark - Public

+ (UIAlertActionStyle)alertStyleForDialogButtonStyle:(DialogButtonStyle)style {
  switch (style) {
    case DialogButtonStyle::DEFAULT:
      return UIAlertActionStyleDefault;
    case DialogButtonStyle::CANCEL:
      return UIAlertActionStyleCancel;
    case DialogButtonStyle::DESTRUCTIVE:
      return UIAlertActionStyleDestructive;
  }
}

#pragma mark - DialogConsumer

- (void)setDialogTitle:(nullable NSString*)title {
  self.title = title;
}

- (void)setDialogMessage:(nullable NSString*)message {
  self.message = message;
}

- (void)setDialogButtonConfigurations:
    (nullable NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  _buttonConfigs = buttonConfigs;
}

- (void)setDialogTextFieldConfigurations:
    (nullable NSArray<DialogTextFieldConfiguration*>*)textFieldConfigs {
  _textFieldConfigs = textFieldConfigs;
}

#pragma mark - UIViewcontroller

- (void)viewDidLoad {
  DCHECK_GT(self.buttonConfigs.count, 0U);
  [self addButtons];
  [self addTextFields];
}

#pragma mark -

- (AlertActionHandler)actionForButtonConfiguration:
    (DialogButtonConfiguration*)buttonConfig {
  return ^(UIAlertAction*) {
    [self.dismissalDispatcher dismissDialogWithButtonID:buttonConfig.identifier
                                        textFieldValues:self.textFieldValues];
  };
}

- (void)addButtons {
  for (DialogButtonConfiguration* config in self.buttonConfigs) {
    AlertActionHandler handler = [self actionForButtonConfiguration:config];
    UIAlertActionStyle style =
        [[self class] alertStyleForDialogButtonStyle:config.style];
    [self addAction:[UIAlertAction actionWithTitle:config.text
                                             style:style
                                           handler:handler]];
  }
}

- (void)addTextFields {
  for (DialogTextFieldConfiguration* config in self.textFieldConfigs) {
    [self addTextFieldWithConfigurationHandler:^(UITextField* textField) {
      textField.text = config.defaultText;
      textField.placeholder = config.placeholderText;
      textField.secureTextEntry = config.secure;
    }];
  }
}

@end
