// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/test/test_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"

ExtensionTestMessageListener::ExtensionTestMessageListener(
    const std::string& expected_message,
    bool will_reply)
    : expected_message_(expected_message),
      satisfied_(false),
      waiting_(false),
      will_reply_(will_reply),
      failure_message_(""),
      failed_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                 content::NotificationService::AllSources());
}

ExtensionTestMessageListener::~ExtensionTestMessageListener() {}

bool ExtensionTestMessageListener::WaitUntilSatisfied()  {
  if (satisfied_)
    return !failed_;
  waiting_ = true;
  content::RunMessageLoop();
  return !failed_;
}

void ExtensionTestMessageListener::Reply(const std::string& message) {
  DCHECK(satisfied_);
  DCHECK(will_reply_);
  function_->Reply(message);
  function_ = NULL;
  will_reply_ = false;
}

void ExtensionTestMessageListener::Reply(int message) {
  Reply(base::IntToString(message));
}

void ExtensionTestMessageListener::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const std::string& content = *content::Details<std::string>(details).ptr();
  if (!satisfied_ && (content == expected_message_ ||
      (!failure_message_.empty() && (content == failure_message_)))) {
    if (!failed_)
      failed_ = (content == failure_message_);
    function_ = content::Source<extensions::TestSendMessageFunction>(
        source).ptr();
    satisfied_ = true;
    registrar_.RemoveAll();  // Stop listening for more messages.
    if (!will_reply_) {
      function_->Reply("");
      function_ = NULL;
    }
    if (waiting_) {
      waiting_ = false;
      MessageLoopForUI::current()->Quit();
    }
  }
}
