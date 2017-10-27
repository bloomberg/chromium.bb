// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_template_builder.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"

using message_center::Notification;
using message_center::NotifierId;
using message_center::RichNotificationData;

namespace {

const char kNotificationId[] = "notification_id";
const char kNotificationTitle[] = "My Title";
const char kNotificationMessage[] = "My Message";
const char kNotificationOrigin[] = "https://example.com";

};

class NotificationTemplateBuilderTest : public ::testing::Test {
 public:
  NotificationTemplateBuilderTest() = default;
  ~NotificationTemplateBuilderTest() override = default;

 protected:
  // Builds a notification object and initializes it to default values.
  std::unique_ptr<message_center::Notification> InitializeBasicNotification() {
    GURL origin_url(kNotificationOrigin);

    return std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
        base::UTF8ToUTF16(kNotificationTitle),
        base::UTF8ToUTF16(kNotificationMessage), gfx::Image() /* icon */,
        base::string16() /* display_source */, origin_url,
        NotifierId(origin_url), RichNotificationData(), nullptr /* delegate */);
  }

  // Converts the notification data to XML and verifies it is as expected. Calls
  // must be wrapped in ASSERT_NO_FATAL_FAILURE().
  void VerifyXml(const message_center::Notification& notification,
                 const base::string16& xml_template) {
    template_ =
        NotificationTemplateBuilder::Build(kNotificationId, notification);
    ASSERT_TRUE(template_);

    EXPECT_EQ(template_->GetNotificationTemplate(), xml_template);
  }

 protected:
  std::unique_ptr<NotificationTemplateBuilder> template_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationTemplateBuilderTest);
};

TEST_F(NotificationTemplateBuilderTest, SimpleToast) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Buttons) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(base::ASCIIToUTF16("Button1"));
  buttons.emplace_back(base::ASCIIToUTF16("Button2"));
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <action activationType="foreground" content="Button1" arguments="buttonIndex=0"/>
  <action activationType="foreground" content="Button2" arguments="buttonIndex=1"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineReplies) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(base::ASCIIToUTF16("Button1"));
  button1.type = message_center::ButtonType::TEXT;
  button1.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button1);
  buttons.emplace_back(base::ASCIIToUTF16("Button2"));
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="buttonIndex=0"/>
  <action activationType="foreground" content="Button2" arguments="buttonIndex=1"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesDoubleInput) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  message_center::ButtonInfo button1(base::ASCIIToUTF16("Button1"));
  button1.type = message_center::ButtonType::TEXT;
  button1.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button1);
  message_center::ButtonInfo button2(base::ASCIIToUTF16("Button2"));
  button2.type = message_center::ButtonType::TEXT;
  button2.placeholder = base::ASCIIToUTF16("Should not appear");
  buttons.emplace_back(button2);
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="buttonIndex=0"/>
  <action activationType="foreground" content="Button2" arguments="buttonIndex=1"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, InlineRepliesTextTypeNotFirst) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();

  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(base::ASCIIToUTF16("Button1"));
  message_center::ButtonInfo button2(base::ASCIIToUTF16("Button2"));
  button2.type = message_center::ButtonType::TEXT;
  button2.placeholder = base::ASCIIToUTF16("Reply here");
  buttons.emplace_back(button2);
  notification->set_buttons(buttons);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
  <input id="userResponse" type="text" placeHolderContent="Reply here"/>
  <action activationType="foreground" content="Button1" arguments="buttonIndex=0"/>
  <action activationType="foreground" content="Button2" arguments="buttonIndex=1"/>
 </actions>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}

TEST_F(NotificationTemplateBuilderTest, Silent) {
  std::unique_ptr<message_center::Notification> notification =
      InitializeBasicNotification();
  notification->set_silent(true);

  const wchar_t kExpectedXml[] =
      LR"(<toast launch="notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text>My Message</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <audio silent="true"/>
</toast>
)";

  ASSERT_NO_FATAL_FAILURE(VerifyXml(*notification, kExpectedXml));
}
