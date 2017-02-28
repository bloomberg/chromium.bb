// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/values.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

namespace {

void ExtractIsRestrictedKeyboard(
    bool* is_restricted,
    std::unique_ptr<base::DictionaryValue> settings) {
  ASSERT_TRUE(settings->GetBoolean("restricted", is_restricted));
}

// Handles "restrictedChanged" message from the test extension - the handler
// responds to the the "restrictedChanged" message with a message indicating
// whether advanced features are enabeled,
class RestrictedChangedHandler : public content::NotificationObserver {
 public:
  explicit RestrictedChangedHandler(VirtualKeyboardDelegate* delegate)
      : delegate_(delegate) {
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE, type);

    std::pair<std::string, bool*>* message_details =
        content::Details<std::pair<std::string, bool*>>(details).ptr();
    if (message_details->first != "restrictedChanged")
      return;

    *message_details->second = true /* listener_will_respond */;

    bool is_restricted = false;
    delegate_->GetKeyboardConfig(
        base::Bind(&ExtractIsRestrictedKeyboard, &is_restricted));

    extensions::TestSendMessageFunction* function =
        content::Source<extensions::TestSendMessageFunction>(source).ptr();
    // The response should contain boolean indicating whether virtual keyboard
    // advanced features are enabled.
    function->Reply(is_restricted ? "false" : "true");
  }

 private:
  VirtualKeyboardDelegate* delegate_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedChangedHandler);
};

}  // namespace

class VirtualKeyboardApiTest : public ShellApiTest {
 public:
  VirtualKeyboardApiTest() {}

  ~VirtualKeyboardApiTest() override = default;

  void SetUp() override {
    feature_session_type_ =
        ScopedCurrentFeatureSessionType(FeatureSessionType::KIOSK);
    ShellApiTest::SetUp();
  }

  void TearDown() override {
    feature_session_type_.reset();
    ShellApiTest::TearDown();
  }

 private:
  std::unique_ptr<base::AutoReset<FeatureSessionType>> feature_session_type_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardApiTest);
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardApiTest, Test) {
  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(browser_context());
  ASSERT_TRUE(api);
  ASSERT_TRUE(api->delegate());

  RestrictedChangedHandler restricted_changed_handler(api->delegate());

  EXPECT_TRUE(RunAppTest("api_test/virtual_keyboard"));
}

}  // namespace extensions
