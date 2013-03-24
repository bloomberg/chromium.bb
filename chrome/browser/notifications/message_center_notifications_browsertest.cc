// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/message_center/message_center.h"

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center_switches.h"
#endif

class MessageCenterNotificationsTest : public InProcessBrowserTest {
 public:
  MessageCenterNotificationsTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // This switch enables the new piping of Notifications through Message
    // Center.
    command_line->AppendSwitch(
      message_center::switches::kEnableRichNotifications);
  }

  MessageCenterNotificationManager* manager() {
    return static_cast<MessageCenterNotificationManager*>(
        g_browser_process->notification_ui_manager());
  }

  message_center::MessageCenter* message_center() {
    return g_browser_process->message_center();
  }

  Profile* profile() { return browser()->profile(); }

  class TestDelegate : public NotificationDelegate {
   public:
    explicit TestDelegate(const std::string& id) : id_(id) {}

    void Display() OVERRIDE { log_ += "Display_"; }
    void Error() OVERRIDE { log_ += "Error_"; }
    void Close(bool by_user) OVERRIDE {
      log_ += "Close_";
      log_ += ( by_user ? "by_user_" : "programmatically_");
    }
    void Click() OVERRIDE { log_ += "Click_"; }
    void ButtonClick(int button_index) OVERRIDE {
      log_ += "ButtonClick_";
      log_ += base::IntToString(button_index) + "_";
    }
    std::string id() const OVERRIDE { return id_; }
    content::RenderViewHost* GetRenderViewHost() const OVERRIDE { return NULL; }

    const std::string& log() { return log_; }

   private:
    virtual ~TestDelegate() {}
    std::string id_;
    std::string log_;

    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  Notification CreateTestNotification(const std::string& id,
                                      TestDelegate** delegate = NULL) {
    TestDelegate* new_delegate = new TestDelegate(id);
    if (delegate) {
      *delegate = new_delegate;
      new_delegate->AddRef();
    }

    return Notification(GURL(),
                        GURL(),
                        ASCIIToUTF16("title"),
                        ASCIIToUTF16("message"),
                        WebKit::WebTextDirectionDefault,
                        EmptyString16(),
                        EmptyString16(),
                        new_delegate);
  }
};

// TODO(rsesek): Implement Message Center on Mac and get these tests passing
// for real. http://crbug.com/179904
#if !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, RetrieveBaseParts) {
  // Make sure comamnd-line switch has an effect.
  EXPECT_TRUE(NotificationUIManager::DelegatesToMessageCenter());
  EXPECT_TRUE(manager());
  EXPECT_TRUE(message_center());
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, BasicAddCancel) {
  manager()->Add(CreateTestNotification("hey"), profile());
  EXPECT_EQ(1u, message_center()->NotificationCount());
  manager()->CancelById("hey");
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, BasicDelegate) {
  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("hey", &delegate), profile());
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("Display_", delegate->log());
  manager()->CancelById("hey");
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("Display_Close_programmatically_", delegate->log());
  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, ButtonClickedDelegate) {
  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("n", &delegate), profile());
  message_center()->OnButtonClicked("n", 1);
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("Display_ButtonClick_1_", delegate->log());
  delegate->Release();
}

#endif  // !defined(OS_MACOSX)
