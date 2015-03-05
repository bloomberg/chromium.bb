// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_MAIL_COMPOSER_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_MAIL_COMPOSER_COMMAND_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

namespace base {
class FilePath;
}

@interface ShowMailComposerCommand : GenericChromeCommand

- (instancetype)initWithToRecipient:(NSString*)toRecipient
                             subject:(NSString*)subject
                                body:(NSString*)body
      emailNotConfiguredAlertTitleId:(int)alertTitleId
    emailNotConfiguredAlertMessageId:(int)alertMessageId;

@property(nonatomic, readonly, assign) NSArray* toRecipients;
@property(nonatomic, readonly, assign) NSString* subject;
@property(nonatomic, readonly, assign) NSString* body;
@property(nonatomic, assign) base::FilePath textFileToAttach;
@property(nonatomic, readonly, assign) int emailNotConfiguredAlertTitleId;
@property(nonatomic, readonly, assign) int emailNotConfiguredAlertMessageId;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_MAIL_COMPOSER_COMMAND_H_
