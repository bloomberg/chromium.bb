// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHOW_MAIL_COMPOSER_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SHOW_MAIL_COMPOSER_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"

// Shows the mail composer UI with the specified parameters.
void ShowMailComposer(const base::string16& to_recipient,
                      const base::string16& subject,
                      const base::string16& body,
                      const base::string16& title,
                      const base::FilePath& text_file_to_attach,
                      int email_not_configured_alert_title_id,
                      int email_not_configured_alert_message_id);

#endif  // IOS_CHROME_BROWSER_UI_SHOW_MAIL_COMPOSER_UTIL_H_
