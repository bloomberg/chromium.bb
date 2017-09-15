// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/unavailable_feature_dialogs/unavailable_feature_dialog_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/clean/chrome/browser/ui/commands/unavailable_feature_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/web/public/url_scheme_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnavailableFeatureDialogMediator ()<DialogDismissalCommands>

// The feature name.
@property(nonatomic, readonly, copy) NSString* featureName;
// The configuration identifier.
@property(nonatomic, weak) DialogConfigurationIdentifier* OKButtonIdentifier;

@end

@implementation UnavailableFeatureDialogMediator
@synthesize dispatcher = _dispatcher;
@synthesize featureName = _featureName;
@synthesize OKButtonIdentifier = _OKButtonIdentifier;

- (instancetype)initWithFeatureName:(NSString*)featureName {
  DCHECK(featureName.length);
  if (self = [super init]) {
    _featureName = [featureName copy];
  }
  return self;
}

#pragma mark - DialogDismissalCommands

- (void)dismissDialogWithButtonID:(DialogConfigurationIdentifier*)buttonID
                  textFieldValues:(DialogTextFieldValues*)textFieldValues {
  DCHECK(buttonID);
  DCHECK_EQ(buttonID, self.OKButtonIdentifier);
  [self.dispatcher dismissUnavailableFeatureDialog];
}

@end

@implementation UnavailableFeatureDialogMediator (DialogMediatorSubclassing)

- (NSString*)dialogTitle {
  return @"Feature Unavailable:";
}

- (NSString*)dialogMessage {
  return self.featureName;
}

- (NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  NSArray<DialogButtonConfiguration*>* configs = @[ [DialogButtonConfiguration
      configWithText:@"OK"
               style:DialogButtonStyle::DEFAULT] ];
  self.OKButtonIdentifier = [configs lastObject].identifier;
  return configs;
}

@end
