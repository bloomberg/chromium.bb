// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/x/x11_util.h"

#if defined(USE_AURA)
#include "chrome/browser/chromeos/notifications/balloon_collection_impl_aura.h"
#include "ui/aura/root_window.h"
#else
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#endif

#if defined(USE_AURA)
typedef class chromeos::BalloonCollectionImplAura BalloonCollectionImplType;
#else
typedef class chromeos::BalloonCollectionImpl BalloonCollectionImplType;
#endif

namespace chromeos {

class SystemNotificationTest : public InProcessBrowserTest {
 public:
  SystemNotificationTest() {}

  void HandleWebUIMessage(const ListValue* value) {
    MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    InProcessBrowserTest::SetUp();
  }

  BalloonCollectionImplType* GetBalloonCollectionImpl() {
    return static_cast<BalloonCollectionImplType*>(
        g_browser_process->notification_ui_manager()->balloon_collection());
  }

  Notification NewMockNotification(const std::string& id) {
    return NewMockNotification(new MockNotificationDelegate(id));
  }

  Notification NewMockNotification(NotificationDelegate* delegate) {
    std::string text = delegate->id();
    return SystemNotificationFactory::Create(
        GURL(), ASCIIToUTF16(text.c_str()), ASCIIToUTF16(text.c_str()),
        delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNotificationTest);
};

IN_PROC_BROWSER_TEST_F(SystemNotificationTest, TestSystemNotification) {
  BalloonCollectionImplType* collection = GetBalloonCollectionImpl();
  scoped_refptr<MockNotificationDelegate> delegate(
      new MockNotificationDelegate("power"));

  EXPECT_EQ(0, collection->count());

  Notification notify = NewMockNotification(delegate.get());
  collection->AddSystemNotification(notify, browser()->profile(), true);

  EXPECT_EQ(1, collection->count());

  Notification update = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated"), delegate.get());
  collection->UpdateNotification(update);

  EXPECT_EQ(1, collection->count());

  Notification update_and_show = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated and shown"),
      delegate.get());
  collection->UpdateAndShowNotification(update_and_show);

  EXPECT_EQ(1, collection->count());

  // Dismiss the notification.
  collection->RemoveById(delegate->id());
  ui_test_utils::RunAllPendingInMessageLoop();

  EXPECT_EQ(0, collection->count());
}

IN_PROC_BROWSER_TEST_F(SystemNotificationTest, TestAddWebUIMessageCallback) {
  BalloonCollectionImplType* collection = GetBalloonCollectionImpl();
  Profile* profile = browser()->profile();

  collection->AddSystemNotification(
      NewMockNotification("1"), profile, false);

  EXPECT_TRUE(collection->AddWebUIMessageCallback(
      NewMockNotification("1"),
      "test",
      base::Bind(&SystemNotificationTest::HandleWebUIMessage,
          base::Unretained(static_cast<SystemNotificationTest*>(this)))));

  // Adding callback for the same message twice should fail.
  EXPECT_FALSE(collection->AddWebUIMessageCallback(
      NewMockNotification("1"),
      "test",
      base::Bind(&SystemNotificationTest::HandleWebUIMessage,
          base::Unretained(static_cast<SystemNotificationTest*>(this)))));

  // Adding callback to nonexistent notification should fail.
  EXPECT_FALSE(collection->AddWebUIMessageCallback(
      NewMockNotification("2"),
      "test1",
      base::Bind(&SystemNotificationTest::HandleWebUIMessage,
          base::Unretained(static_cast<SystemNotificationTest*>(this)))));
}

// Occasional crash: http://crbug.com/96461
IN_PROC_BROWSER_TEST_F(SystemNotificationTest, TestWebUIMessageCallback) {
  BalloonCollectionImplType* collection = GetBalloonCollectionImpl();
  Profile* profile = browser()->profile();
  // A notification that sends 'test' WebUI message back to chrome.
  const GURL content_url(
      "data:text/html;charset=utf-8,"
      "<html><script>function send() { chrome.send('test', ['']); }</script>"
      "<body onload='send()'></body></html>");
  collection->AddSystemNotification(
      Notification(GURL(), content_url, string16(), string16(),
                   new MockNotificationDelegate("1")),
      profile,
      false);
  EXPECT_TRUE(collection->AddWebUIMessageCallback(
      NewMockNotification("1"),
      "test",
      base::Bind(&SystemNotificationTest::HandleWebUIMessage,
          base::Unretained(static_cast<SystemNotificationTest*>(this)))));
  MessageLoop::current()->Run();
}

}  // namespace chromeos
