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

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

// Initializes a command designed to open the mail composer with pre-filled
// recipients, subject, body.
- (instancetype)initWithToRecipient:(NSString*)toRecipient
                             subject:(NSString*)subject
                                body:(NSString*)body
      emailNotConfiguredAlertTitleId:(int)alertTitleId
    emailNotConfiguredAlertMessageId:(int)alertMessageId
    NS_DESIGNATED_INITIALIZER;

// List of email recipients.
@property(strong, nonatomic, readonly) NSArray* toRecipients;

// Pre-filled default email subject.
@property(copy, nonatomic, readonly) NSString* subject;

// Pre-filled default email body.
@property(copy, nonatomic, readonly) NSString* body;

// Path to file to attach to email.
@property(nonatomic, assign) const base::FilePath& textFileToAttach;

// Identifier for alert if the email title is empty.
@property(nonatomic, readonly) int emailNotConfiguredAlertTitleId;

// Identifier for alert if the email body is empty.
@property(nonatomic, readonly) int emailNotConfiguredAlertMessageId;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_MAIL_COMPOSER_COMMAND_H_
