// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"

#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"

// TODOf remove.
#include <iostream>

ExtensionTestMessageListener::ExtensionTestMessageListener(
    const std::string& expected_message)
    : expected_message_(expected_message), satisfied_(false),
      waiting_(false) {
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

void ExtensionTestMessageListener::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const std::string& content = *Details<std::string>(details).ptr();
  // TODOf remove.
  std::cout << "**** Observed: '" << content.c_str() << "', expected: '" << expected_message_.c_str() << "'\n" << std::flush;
  if (!satisfied_ && content == expected_message_) {
    satisfied_ = true;
    registrar_.RemoveAll();  // Stop listening for more messages.
    if (waiting_) {
      waiting_ = false;
      MessageLoopForUI::current()->Quit();
    }
  }
}
