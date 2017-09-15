// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_request.h"
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

@interface ContextMenuDialogMediator ()<DialogDismissalCommands>

// The request passed on initializaton.
@property(nonatomic, readonly, strong) ContextMenuDialogRequest* request;
// The configuration identifiers.
@property(nonatomic, weak) DialogConfigurationIdentifier* executeScriptButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* openInNewTabButtonID;
@property(nonatomic, weak)
    DialogConfigurationIdentifier* openInNewIncognitoTabButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* linkCopyButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* saveImageButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* openImageButtonID;
@property(nonatomic, weak)
    DialogConfigurationIdentifier* openImageInNewTabButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* cancelButtonID;

@end

@implementation ContextMenuDialogMediator
@synthesize dispatcher = _dispatcher;
@synthesize request = _request;
@synthesize executeScriptButtonID = _executeScriptButtonID;
@synthesize openInNewTabButtonID = _openInNewTabButtonID;
@synthesize openInNewIncognitoTabButtonID = _openInNewIncognitoTabButtonID;
@synthesize linkCopyButtonID = _linkCopyButtonID;
@synthesize saveImageButtonID = _saveImageButtonID;
@synthesize openImageButtonID = _openImageButtonID;
@synthesize openImageInNewTabButtonID = _openImageInNewTabButtonID;
@synthesize cancelButtonID = _cancelButtonID;

- (instancetype)initWithRequest:(ContextMenuDialogRequest*)request {
  DCHECK(request);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - DialogDismissalCommands

- (void)dismissDialogWithButtonID:(DialogConfigurationIdentifier*)buttonID
                  textFieldValues:(DialogTextFieldValues*)textFieldValues {
  DCHECK(buttonID);
  NSString* unavailableFeatureName = nil;
  BOOL changesActiveWebState = NO;
  if (buttonID == self.executeScriptButtonID) {
    [self.dispatcher executeContextMenuScript:self.request];
  } else if (buttonID == self.openInNewTabButtonID) {
    [self.dispatcher openContextMenuLinkInNewTab:self.request];
    changesActiveWebState = YES;
  } else if (buttonID == self.openInNewIncognitoTabButtonID) {
    // TODO(crbug.com/760644): Dispatch command once incognito is implemented.
    unavailableFeatureName = @"Open In New Incognito Tab";
  } else if (buttonID == self.linkCopyButtonID) {
    // TODO(crbug.com/760644): Dispatch command once pasteboard support is
    // implemented.
    unavailableFeatureName = @"Copy Link";
  } else if (buttonID == self.saveImageButtonID) {
    // TODO(crbug.com/760644): Dispatch command once image saving is
    // implemented.
    unavailableFeatureName = @"Save Image";
  } else if (buttonID == self.openImageButtonID) {
    [self.dispatcher openContextMenuImage:self.request];
  } else if (buttonID == self.openImageInNewTabButtonID) {
    [self.dispatcher openContextMenuImageInNewTab:self.request];
    changesActiveWebState = YES;
  } else if (buttonID != self.cancelButtonID) {
    NOTREACHED() << "Received dialog dismissal for unknown button tag.";
  }
  if (unavailableFeatureName) {
    [self.dispatcher
        dismissContextMenuForUnavailableFeatureNamed:unavailableFeatureName];
  } else if (!changesActiveWebState) {
    [self.dispatcher dismissContextMenu];
  }
}

@end

@implementation ContextMenuDialogMediator (DialogMediatorSubclassing)

- (NSString*)dialogTitle {
  return self.request.menuTitle;
}

- (NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  NSMutableArray<DialogButtonConfiguration*>* configs = [NSMutableArray array];
  // Add the script execution button if this was a JavaScript URL.
  if (self.request.script.size()) {
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Execute Script"
                                    style:DialogButtonStyle::DEFAULT]];
    self.executeScriptButtonID = [configs lastObject].identifier;
  }
  // Add link buttons if the link URL is valid.
  BOOL addLinkButtons = self.request.linkURL.is_valid() &&
                        web::UrlHasWebScheme(self.request.linkURL);
  if (addLinkButtons) {
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Open In New Tab"
                                    style:DialogButtonStyle::DEFAULT]];
    self.openInNewTabButtonID = [configs lastObject].identifier;
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Open In New Incognito Tab"
                                    style:DialogButtonStyle::DEFAULT]];
    self.openInNewIncognitoTabButtonID = [configs lastObject].identifier;
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Copy Link"
                                    style:DialogButtonStyle::DEFAULT]];
    self.linkCopyButtonID = [configs lastObject].identifier;
  }
  // Add image buttons if the image URL is valid.
  if (self.request.imageURL.is_valid()) {
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Save Image"
                                    style:DialogButtonStyle::DEFAULT]];
    self.saveImageButtonID = [configs lastObject].identifier;
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Open Image"
                                    style:DialogButtonStyle::DEFAULT]];
    self.openImageButtonID = [configs lastObject].identifier;
    [configs addObject:[DialogButtonConfiguration
                           configWithText:@"Open Image In New Tab"
                                    style:DialogButtonStyle::DEFAULT]];
    self.openImageInNewTabButtonID = [configs lastObject].identifier;
  }
  // Add the cancel button last.
  [configs addObject:[DialogButtonConfiguration
                         configWithText:@"Cancel"
                                  style:DialogButtonStyle::CANCEL]];
  self.cancelButtonID = [configs lastObject].identifier;
  return [configs copy];
}

@end
