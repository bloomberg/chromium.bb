// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/show_mail_composer_command.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowMailComposerCommand {
  base::FilePath _textFileToAttach;
}

@synthesize emailNotConfiguredAlertTitleId = _emailNotConfiguredAlertTitleId;
@synthesize emailNotConfiguredAlertMessageId =
    _emailNotConfiguredAlertMessageId;
@synthesize toRecipients = _toRecipients;
@synthesize subject = _subject;
@synthesize body = _body;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithToRecipient:(NSString*)toRecipient
                             subject:(NSString*)subject
                                body:(NSString*)body
      emailNotConfiguredAlertTitleId:(int)alertTitleId
    emailNotConfiguredAlertMessageId:(int)alertMessageId {
  DCHECK(alertTitleId);
  DCHECK(alertMessageId);
  self = [super initWithTag:IDC_SHOW_MAIL_COMPOSER];
  if (self) {
    _toRecipients = @[ toRecipient ];
    _subject = [subject copy];
    _body = [body copy];
    _emailNotConfiguredAlertTitleId = alertTitleId;
    _emailNotConfiguredAlertMessageId = alertMessageId;
  }
  return self;
}

- (const base::FilePath&)textFileToAttach {
  return _textFileToAttach;
}

- (void)setTextFileToAttach:(const base::FilePath&)textFileToAttach {
  _textFileToAttach = textFileToAttach;
}

@end
