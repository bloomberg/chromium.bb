// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"

class TestAddObserver : public message_center::MessageCenterObserver {
 public:
  explicit TestAddObserver(message_center::MessageCenter* message_center)
      : message_center_(message_center) {
    message_center_->AddObserver(this);
  }

  virtual ~TestAddObserver() { message_center_->RemoveObserver(this); }

  virtual void OnNotificationAdded(const std::string& id) OVERRIDE {
    std::string log = logs_[id];
    if (log != "")
      log += "_";
    logs_[id] = log + "add-" + id;
  }

  virtual void OnNotificationUpdated(const std::string& id) OVERRIDE {
    std::string log = logs_[id];
    if (log != "")
      log += "_";
    logs_[id] = log + "update-" + id;
  }

  const std::string log(const std::string& id) { return logs_[id]; }
  void reset_logs() { logs_.clear(); }

 private:
  std::map<std::string, std::string> logs_;
  message_center::MessageCenter* message_center_;
};

class MessageCenterNotificationsTest : public InProcessBrowserTest {
 public:
  MessageCenterNotificationsTest() {}

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

    virtual void Display() OVERRIDE { log_ += "Display_"; }
    virtual void Error() OVERRIDE { log_ += "Error_"; }
    virtual void Close(bool by_user) OVERRIDE {
      log_ += "Close_";
      log_ += ( by_user ? "by_user_" : "programmatically_");
    }
    virtual void Click() OVERRIDE { log_ += "Click_"; }
    virtual void ButtonClick(int button_index) OVERRIDE {
      log_ += "ButtonClick_";
      log_ += base::IntToString(button_index) + "_";
    }
    virtual std::string id() const OVERRIDE { return id_; }
    virtual content::WebContents* GetWebContents() const OVERRIDE {
      return NULL;
    }

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

    return Notification(GURL("chrome-test://testing/"),
                        GURL(),
                        base::ASCIIToUTF16("title"),
                        base::ASCIIToUTF16("message"),
                        blink::WebTextDirectionDefault,
                        base::UTF8ToUTF16("chrome-test://testing/"),
                        base::UTF8ToUTF16("REPLACE-ME"),
                        new_delegate);
  }

  Notification CreateRichTestNotification(const std::string& id,
                                          TestDelegate** delegate = NULL) {
    TestDelegate* new_delegate = new TestDelegate(id);
    if (delegate) {
      *delegate = new_delegate;
      new_delegate->AddRef();
    }

    message_center::RichNotificationData data;

    return Notification(message_center::NOTIFICATION_TYPE_BASE_FORMAT,
                        GURL("chrome-test://testing/"),
                        base::ASCIIToUTF16("title"),
                        base::ASCIIToUTF16("message"),
                        gfx::Image(),
                        blink::WebTextDirectionDefault,
                        message_center::NotifierId(
                            message_center::NotifierId::APPLICATION,
                            "extension_id"),
                        base::UTF8ToUTF16("chrome-test://testing/"),
                        base::UTF8ToUTF16("REPLACE-ME"),
                        data,
                        new_delegate);
  }
};

// TODO(rsesek): Implement Message Center on Mac and get these tests passing
// for real. http://crbug.com/179904
#if !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, RetrieveBaseParts) {
  EXPECT_TRUE(manager());
  EXPECT_TRUE(message_center());
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, BasicAddCancel) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // Someone may create system notifications like "you're in multi-profile
  // mode..." or something which may change the expectation.
  // TODO(mukai): move this to SetUpOnMainThread() after fixing the side-effect
  // of canceling animation which prevents some Displayed() event.
  manager()->CancelAll();
  manager()->Add(CreateTestNotification("hey"), profile());
  EXPECT_EQ(1u, message_center()->NotificationCount());
  manager()->CancelById("hey");
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, BasicDelegate) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("n", &delegate), profile());
  message_center()->ClickOnNotificationButton("n", 1);
  // Verify that delegate accumulated correct log of events.
  EXPECT_EQ("Display_ButtonClick_1_", delegate->log());
  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest,
                       UpdateExistingNotification) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestDelegate* delegate;
  manager()->Add(CreateTestNotification("n", &delegate), profile());
  TestDelegate* delegate2;
  manager()->Add(CreateRichTestNotification("n", &delegate2), profile());

  manager()->CancelById("n");
  EXPECT_EQ("Display_", delegate->log());
  EXPECT_EQ("Close_programmatically_", delegate2->log());

  delegate->Release();
  delegate2->Release();
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest, QueueWhenCenterVisible) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestAddObserver observer(message_center());

  TestDelegate* delegate;
  TestDelegate* delegate2;

  manager()->Add(CreateTestNotification("n", &delegate), profile());
  message_center()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  manager()->Add(CreateTestNotification("n2", &delegate2), profile());

  // 'update-n' should happen since SetVisibility updates is_read status of n.
  // TODO(mukai): fix event handling to happen update-n just once.
  EXPECT_EQ("add-n_update-n_update-n", observer.log("n"));

  message_center()->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  EXPECT_EQ("add-n2", observer.log("n2"));

  delegate->Release();
  delegate2->Release();
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest,
                       UpdateNonProgressNotificationWhenCenterVisible) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestAddObserver observer(message_center());

  TestDelegate* delegate;

  // Add a non-progress notification and update it while the message center
  // is visible.
  Notification notification = CreateTestNotification("n", &delegate);
  manager()->Add(notification, profile());
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification.set_title(base::ASCIIToUTF16("title2"));
  manager()->Update(notification, profile());

  // Expect that the notification update is not done.
  EXPECT_EQ("", observer.log("n"));

  message_center()->SetVisibility(message_center::VISIBILITY_TRANSIENT);
  EXPECT_EQ("update-n", observer.log("n"));

  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(
    MessageCenterNotificationsTest,
    UpdateNonProgressToProgressNotificationWhenCenterVisible) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestAddObserver observer(message_center());

  TestDelegate* delegate;

  // Add a non-progress notification and change the type to progress while the
  // message center is visible.
  Notification notification = CreateTestNotification("n", &delegate);
  manager()->Add(notification, profile());
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  manager()->Update(notification, profile());

  // Expect that the notification update is not done.
  EXPECT_EQ("", observer.log("n"));

  message_center()->SetVisibility(message_center::VISIBILITY_TRANSIENT);
  EXPECT_EQ("update-n", observer.log("n"));

  delegate->Release();
}

IN_PROC_BROWSER_TEST_F(MessageCenterNotificationsTest,
                       UpdateProgressNotificationWhenCenterVisible) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  TestAddObserver observer(message_center());

  TestDelegate* delegate;

  // Add a progress notification and update it while the message center
  // is visible.
  Notification notification = CreateTestNotification("n", &delegate);
  notification.set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  manager()->Add(notification, profile());
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification.set_progress(50);
  manager()->Update(notification, profile());

  // Expect that the progress notification update is performed.
  EXPECT_EQ("update-n", observer.log("n"));

  delegate->Release();
}

#endif  // !defined(OS_MACOSX)
