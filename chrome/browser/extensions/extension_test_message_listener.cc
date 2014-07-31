// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"

ExtensionTestMessageListener::ExtensionTestMessageListener(
    const std::string& expected_message,
    bool will_reply)
    : expected_message_(expected_message),
      satisfied_(false),
      waiting_(false),
      wait_for_any_message_(false),
      will_reply_(will_reply),
      replied_(false),
      failed_(false) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                 content::NotificationService::AllSources());
}

ExtensionTestMessageListener::ExtensionTestMessageListener(bool will_reply)
    : satisfied_(false),
      waiting_(false),
      wait_for_any_message_(true),
      will_reply_(will_reply),
      replied_(false),
      failed_(false) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
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
  CHECK(satisfied_);
  CHECK(!replied_);

  replied_ = true;
  function_->Reply(message);
  function_ = NULL;
}

void ExtensionTestMessageListener::Reply(int message) {
  Reply(base::IntToString(message));
}

void ExtensionTestMessageListener::ReplyWithError(const std::string& error) {
  CHECK(satisfied_);
  CHECK(!replied_);

  replied_ = true;
  function_->ReplyWithError(error);
  function_ = NULL;
}

void ExtensionTestMessageListener::Reset() {
  satisfied_ = false;
  failed_ = false;
  message_.clear();
  replied_ = false;
}

void ExtensionTestMessageListener::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE, type);

  // Return immediately if we're already satisfied or it's not the right
  // extension.
  extensions::TestSendMessageFunction* function =
      content::Source<extensions::TestSendMessageFunction>(source).ptr();
  if (satisfied_ ||
      (!extension_id_.empty() && function->extension_id() != extension_id_)) {
    return;
  }

  // We should have an empty message if we're not already satisfied.
  CHECK(message_.empty());

  const std::string& message = *content::Details<std::string>(details).ptr();
  if (message == expected_message_ || wait_for_any_message_ ||
      (!failure_message_.empty() && message == failure_message_)) {
    message_ = message;
    satisfied_ = true;
    failed_ = (message_ == failure_message_);

    // Reply immediately, or save the function for future use.
    function_ = function;
    if (!will_reply_)
      Reply(std::string());

    if (waiting_) {
      waiting_ = false;
      base::MessageLoopForUI::current()->Quit();
    }
  }
}
