// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"

#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"

// TODO(finnur): Remove after capturing debug info.
#include <iostream>
#include "chrome/common/extensions/extension.h"

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
  function_ = Source<ExtensionTestSendMessageFunction>(source).ptr();
  // TODO(finnur): Remove after capturing debug info.
  if (Extension::emit_traces_for_whitelist_extension_test_) {
    std::cout << "-*-*- Got     : " << content.c_str() << "\n" << std::flush;
    std::cout << "-*-*- Expected: " << expected_message_.c_str() << "\n" << std::flush;
  }

  if (!satisfied_ && content == expected_message_) {
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
