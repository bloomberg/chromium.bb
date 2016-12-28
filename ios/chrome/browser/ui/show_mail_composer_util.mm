// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/show_mail_composer_util.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/show_mail_composer_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void ShowMailComposer(const base::string16& to_recipient,
                      const base::string16& subject,
                      const base::string16& body,
                      const base::string16& title,
                      const base::FilePath& text_file_to_attach,
                      int email_not_configured_alert_title_id,
                      int email_not_configured_alert_message_id) {
  ShowMailComposerCommand* command = [[ShowMailComposerCommand alloc]
                   initWithToRecipient:base::SysUTF16ToNSString(to_recipient)
                               subject:base::SysUTF16ToNSString(subject)
                                  body:base::SysUTF16ToNSString(body)
        emailNotConfiguredAlertTitleId:email_not_configured_alert_title_id
      emailNotConfiguredAlertMessageId:email_not_configured_alert_message_id];
  [command setTextFileToAttach:text_file_to_attach];
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  DCHECK(main_window);
  [main_window.rootViewController chromeExecuteCommand:command];
}
