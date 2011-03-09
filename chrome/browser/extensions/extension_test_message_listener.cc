// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"

#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/test/ui_test_utils.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

ExtensionTestMessageListener::ExtensionTestMessageListener(
    const std::string& expected_message,
    bool will_reply)
    : expected_message_(expected_message),
      satisfied_(false),
      waiting_(false),
      will_reply_(will_reply) {
  registrar_.Add(this, NotificationType::EXTENSION_TEST_MESSAGE,
                 NotificationService::AllSources());
}

ExtensionTestMessageListener::~ExtensionTestMessageListener() {}

bool ExtensionTestMessageListener::WaitUntilSatisfied()  {
  if (satisfied_)
    return true;
  waiting_ = true;
  ui_test_utils::RunMessageLoop();
  return satisfied_;
}

void ExtensionTestMessageListener::Reply(const std::string& message) {
  DCHECK(satisfied_);
  DCHECK(will_reply_);
  function_->Reply(message);
  function_ = NULL;
  will_reply_ = false;
}

void ExtensionTestMessageListener::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const std::string& content = *Details<std::string>(details).ptr();
  if (!satisfied_ && content == expected_message_) {
    function_ = Source<ExtensionTestSendMessageFunction>(source).ptr();
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
