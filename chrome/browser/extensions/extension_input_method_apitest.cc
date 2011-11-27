// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kNewInputMethod[] = "ru::rus";
const char kSetInputMethodMessage[] = "setInputMethod";
const char kSetInputMethodDone[] = "done";

// Class that listens for the JS message then changes input method and replies
// back.
class SetInputMethodListener : public content::NotificationObserver {
 public:
  // Creates listener, which should reply exactly |count_| times.
  explicit SetInputMethodListener(int count) : count_(count) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  virtual ~SetInputMethodListener() {
    EXPECT_EQ(0, count_);
  }

  // Implements the content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    const std::string& content = *content::Details<std::string>(details).ptr();
    const std::string expected_message = StringPrintf("%s:%s",
                                                      kSetInputMethodMessage,
                                                      kNewInputMethod);
    if (content == expected_message) {
      chromeos::input_method::InputMethodManager::GetInstance()->
          ChangeInputMethod(StringPrintf("xkb:%s", kNewInputMethod));

      ExtensionTestSendMessageFunction* function =
          content::Source<ExtensionTestSendMessageFunction>(source).ptr();
      EXPECT_GT(count_--, 0);
      function->Reply(kSetInputMethodDone);
    }
  }

 private:
  content::NotificationRegistrar registrar_;

  int count_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, InputMethodApiBasic) {
  // Two test, two calls. See JS code for more info.
  SetInputMethodListener listener(2);

  ASSERT_TRUE(RunExtensionTest("input_method")) << message_;
}
