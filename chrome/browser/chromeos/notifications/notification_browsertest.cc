// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "chrome/browser/chromeos/notifications/notification_panel.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "ui/base/x/x11_util.h"

namespace {

// The name of ChromeOS's window manager.
const char* kChromeOsWindowManagerName = "chromeos-wm";

}  // namespace

namespace chromeos {

class NotificationTest : public InProcessBrowserTest,
                         public NotificationObserver {
 public:
  NotificationTest()
      : under_chromeos_(false),
        state_(PanelController::INITIAL),
        expected_(PanelController::INITIAL) {
  }

  void HandleDOMUIMessage(const ListValue* value) {
    MessageLoop::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    // Detect if we're running under ChromeOS WindowManager. See
    // the description for "under_chromeos_" below for why we need this.
    std::string wm_name;
    bool wm_name_valid = ui::GetWindowManagerName(&wm_name);
    // NOTE: On Chrome OS the wm and Chrome are started in parallel. This
    // means it's possible for us not to be able to get the name of the window
    // manager. We assume that when this happens we're on Chrome OS.
    under_chromeos_ = (!wm_name_valid ||
                       wm_name == kChromeOsWindowManagerName);
    InProcessBrowserTest::SetUp();
  }

  BalloonCollectionImpl* GetBalloonCollectionImpl() {
    return static_cast<BalloonCollectionImpl*>(
        g_browser_process->notification_ui_manager()->balloon_collection());
  }

  NotificationPanel* GetNotificationPanel() {
    return static_cast<NotificationPanel*>(
        GetBalloonCollectionImpl()->notification_ui());
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

  void MarkStale(const char* id) {
    GetNotificationPanel()->GetTester()->MarkStale(NewMockNotification(id));
  }

  // Waits untilt the panel's state becomes the specified state.
  // Does nothing if it's not running with ChromeOS Window Manager.
  void WaitForPanelState(NotificationPanelTester* tester,
                         PanelController::State state) {
    if (under_chromeos_ && state != state_) {
      expected_ = state;
      ui_test_utils::RunAllPendingInMessageLoop();
    }
  }

  // Busy loop to wait until the webkit give some size to the notification.
  void WaitForResize(BalloonViewImpl* view) {
    while (view->bounds().IsEmpty()) {
      ui_test_utils::RunAllPendingInMessageLoop();
    }
  }

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ASSERT_TRUE(NotificationType::PANEL_STATE_CHANGED == type);
    PanelController::State* state =
        reinterpret_cast<PanelController::State*>(details.map_key());
    state_ = *state;
    if (under_chromeos_ && expected_ == state_) {
      expected_ = PanelController::INITIAL;
      MessageLoop::current()->Quit();
    }
  }

 private:
  // ChromeOS build of chrome communicates with ChromeOS's
  // WindowManager, and behaves differently if it runs under a
  // chromeos window manager.  ChromeOS WindowManager sends
  // EXPANDED/MINIMIED state change message when the panels's state
  // changed (regardless of who changed it), and to avoid
  // mis-recognizing such events as user-initiated actions, we need to
  // wait and eat them before moving to a next step.
  bool under_chromeos_;
  PanelController::State state_;
  PanelController::State expected_;
};

IN_PROC_BROWSER_TEST_F(NotificationTest, TestBasic) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  // Using system notification as regular notification.
  collection->Add(NewMockNotification("1"), browser()->profile());

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(0, tester->GetStickyNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->Add(NewMockNotification("2"), browser()->profile());

  EXPECT_EQ(2, tester->GetNewNotificationCount());
  EXPECT_EQ(2, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->RemoveById("1");
  ui_test_utils::RunAllPendingInMessageLoop();

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->RemoveById("2");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0, tester->GetNewNotificationCount());
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  // CLOSE is asynchronous. Run the all pending tasks to finish closing
  // task.
  ui_test_utils::RunAllPendingInMessageLoop();
}

// [CLOSED] -add->[STICKY_AND_NEW] -mouse-> [KEEP_SIZE] -remove/add->
// [KEEP_SIZE] -remove-> [CLOSED] -add-> [STICKY_AND_NEW] -remove-> [CLOSED]
IN_PROC_BROWSER_TEST_F(NotificationTest, TestKeepSizeState) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  // Using system notification as regular notification.
  collection->Add(NewMockNotification("1"), browser()->profile());
  collection->Add(NewMockNotification("2"), browser()->profile());

  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  panel->OnMouseMotion(gfx::Point(10, 10));
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->RemoveById("1");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->Add(NewMockNotification("1"), browser()->profile());
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(2, tester->GetNewNotificationCount());
  EXPECT_EQ(2, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->RemoveById("1");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::KEEP_SIZE, tester->state());

  collection->RemoveById("2");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  collection->Add(NewMockNotification("3"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  collection->RemoveById("3");

  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestSystemNotification) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  scoped_refptr<MockNotificationDelegate> delegate(
      new MockNotificationDelegate("power"));
  NotificationPanelTester* tester = panel->GetTester();

  Notification notify = NewMockNotification(delegate.get());
  collection->AddSystemNotification(notify, browser()->profile(), true, false);

  EXPECT_EQ(1, tester->GetNewNotificationCount());
  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  Notification update = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated"), delegate.get());
  collection->UpdateNotification(update);

  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  Notification update_and_show = SystemNotificationFactory::Create(
      GURL(), ASCIIToUTF16("Title"), ASCIIToUTF16("updated and shown"),
      delegate.get());
  collection->UpdateAndShowNotification(update_and_show);

  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  // Dismiss the notification.
  collection->RemoveById(delegate->id());
  ui_test_utils::RunAllPendingInMessageLoop();

  EXPECT_EQ(0, tester->GetStickyNotificationCount());
  EXPECT_EQ(0, tester->GetNewNotificationCount());
  // TODO(oshima): check content, etc..
}

// [CLOSED] -add,add->[STICKY_AND_NEW] -stale-> [MINIMIZED] -remove->
// [MINIMIZED] -remove-> [CLOSED]
IN_PROC_BROWSER_TEST_F(NotificationTest, TestStateTransition1) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  tester->SetStaleTimeout(0);
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  collection->Add(NewMockNotification("1"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->Add(NewMockNotification("2"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::MINIMIZED, tester->state());

  collection->RemoveById("2");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::MINIMIZED, tester->state());

  collection->RemoveById("1");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  ui_test_utils::RunAllPendingInMessageLoop();
}

// [CLOSED] -add->[STICKY_AND_NEW] -stale-> [MINIMIZED] -add->
// [STICKY_AND_NEW] -stale-> [MINIMIZED] -add sys-> [STICKY_NEW]
// -stale-> [STICKY_NEW] -remove-> [STICKY_NEW] -remove sys->
// [MINIMIZED] -remove-> [CLOSED]
//
// This test depends on the fact that the panel state change occurs
// quicker than stale timeout, thus the stale timeout cannot be set to
// 0. This test explicitly controls the stale state instead.
IN_PROC_BROWSER_TEST_F(NotificationTest, TestStateTransition2) {
  // Register observer here as the registration does not work in SetUp().
  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::PANEL_STATE_CHANGED,
                NotificationService::AllSources());

  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  // See description above.
  tester->SetStaleTimeout(100000);

  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  collection->Add(NewMockNotification("1"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  ui_test_utils::RunAllPendingInMessageLoop();

  // Make the notification stale and make sure panel is minimized state.
  MarkStale("1");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::MINIMIZED, tester->state());
  WaitForPanelState(tester, PanelController::MINIMIZED);

  // Adding new notification expands the panel.
  collection->Add(NewMockNotification("2"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  WaitForPanelState(tester, PanelController::EXPANDED);

  // The panel must be minimzied when the new notification becomes stale.
  MarkStale("2");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::MINIMIZED, tester->state());
  WaitForPanelState(tester, PanelController::MINIMIZED);

  // The panel must be expanded again when a new system notification is added.
  collection->AddSystemNotification(
      NewMockNotification("3"), browser()->profile(), true, false);
  EXPECT_EQ(3, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  WaitForPanelState(tester, PanelController::EXPANDED);

  // Running all events nor removing non sticky should not change the state.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  collection->RemoveById("1");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  // Removing the system notification should minimize the panel.
  collection->RemoveById("3");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::MINIMIZED, tester->state());
  WaitForPanelState(tester, PanelController::MINIMIZED);

  // Removing the last notification. Should close the panel.
  collection->RemoveById("2");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0, tester->GetNotificationCount());
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());

  ui_test_utils::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestCleanupOnExit) {
  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::PANEL_STATE_CHANGED,
                NotificationService::AllSources());

  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();

  // Don't become stale.
  tester->SetStaleTimeout(100000);

  collection->Add(NewMockNotification("1"), browser()->profile());
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  WaitForPanelState(tester, PanelController::EXPANDED);
  // end without closing.
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestCloseOpen) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();
  Profile* profile = browser()->profile();

  collection->Add(NewMockNotification("1"), profile);
  collection->Add(NewMockNotification("2"), profile);
  ui_test_utils::RunAllPendingInMessageLoop();
  WaitForPanelState(tester, PanelController::EXPANDED);
  PanelController* controller = tester->GetPanelController();
  // close now
  panel->ClosePanel();
  controller->Close();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());
  // open again
  collection->Add(NewMockNotification("3"), profile);
  WaitForPanelState(tester, PanelController::EXPANDED);
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());

  // close again
  controller = tester->GetPanelController();
  panel->ClosePanel();
  controller->Close();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::CLOSED, tester->state());
}

// TODO(oshima): bug chromium-os:7139 Fix this flaky test on ChromeOS.
IN_PROC_BROWSER_TEST_F(NotificationTest, FLAKY_TestScrollBalloonToVisible) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();
  Profile* profile = browser()->profile();

  // Create notifications enough to overflow the panel size.
  const int create_count = 15;

  // new notification is always visible
  for (int i = 0; i < create_count; i++) {
    {
      SCOPED_TRACE(base::StringPrintf("new n%d", i));
      std::string id = base::StringPrintf("n%d", i);
      collection->Add(NewMockNotification(id), profile);
      EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
      BalloonViewImpl* view =
          tester->GetBalloonView(collection, NewMockNotification(id));
      WaitForResize(view);
      EXPECT_TRUE(tester->IsVisible(view));
    }
    {
      SCOPED_TRACE(base::StringPrintf("new s%d", i));
      std::string id = base::StringPrintf("s%d", i);
      collection->AddSystemNotification(
          NewMockNotification(id), browser()->profile(), true, false);
      ui_test_utils::RunAllPendingInMessageLoop();
      BalloonViewImpl* view =
          tester->GetBalloonView(collection, NewMockNotification(id));
      WaitForResize(view);
      EXPECT_TRUE(tester->IsVisible(view));
    }
  }
  // Update should not change the visibility
  for (int i = 0; i < create_count; i++) {
    {
      SCOPED_TRACE(base::StringPrintf("update n%d", i));
      Notification notify = NewMockNotification(base::StringPrintf("n%d", i));
      // The last shown notification is sticky, which makes all non sticky
      // invisible.
      EXPECT_TRUE(collection->UpdateNotification(notify));
      ui_test_utils::RunAllPendingInMessageLoop();
      BalloonViewImpl* view = tester->GetBalloonView(collection, notify);
      EXPECT_FALSE(tester->IsVisible(view));
    }
    {
      SCOPED_TRACE(base::StringPrintf("update s%d", i));
      Notification notify = NewMockNotification(base::StringPrintf("s%d", i));
      BalloonViewImpl* view = tester->GetBalloonView(collection, notify);
      bool currently_visible = tester->IsVisible(view);
      EXPECT_TRUE(collection->UpdateNotification(notify));
      ui_test_utils::RunAllPendingInMessageLoop();
      EXPECT_EQ(view, tester->GetBalloonView(collection, notify));
      EXPECT_EQ(currently_visible, tester->IsVisible(view));
    }
  }
  // UpdateAndShowNotification makes notification visible
  for (int i = 0; i < create_count; i++) {
    {
      SCOPED_TRACE(base::StringPrintf("update and show n%d", i));
      Notification notify = NewMockNotification(base::StringPrintf("n%d", i));
      EXPECT_TRUE(collection->UpdateAndShowNotification(notify));
      ui_test_utils::RunAllPendingInMessageLoop();
      BalloonViewImpl* view = tester->GetBalloonView(collection, notify);
      EXPECT_TRUE(tester->IsVisible(view));
    }
    {
      SCOPED_TRACE(base::StringPrintf("update and show s%d", i));
      Notification notify = NewMockNotification(base::StringPrintf("s%d", i));
      EXPECT_TRUE(collection->UpdateAndShowNotification(notify));
      ui_test_utils::RunAllPendingInMessageLoop();
      BalloonViewImpl* view = tester->GetBalloonView(collection, notify);
      EXPECT_TRUE(tester->IsVisible(view));
    }
  }
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestActivateDeactivate) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();
  Profile* profile = browser()->profile();

  collection->Add(NewMockNotification("1"), profile);
  collection->AddSystemNotification(
      NewMockNotification("2"), profile, true, false);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  BalloonViewImpl* view1 =
      tester->GetBalloonView(collection, NewMockNotification("1"));
  BalloonViewImpl* view2 =
      tester->GetBalloonView(collection, NewMockNotification("2"));
  // Wait until all renderers get size.
  WaitForResize(view1);
  WaitForResize(view2);
  EXPECT_LT(view2->size().height(), 50) << "view size is bigger than expected";

  panel->OnMouseMotion(gfx::Point(10, 50));
  EXPECT_TRUE(tester->IsActive(view1));
  EXPECT_FALSE(tester->IsActive(view2));

  panel->OnMouseMotion(gfx::Point(10, 10));
  EXPECT_FALSE(tester->IsActive(view1));
  EXPECT_TRUE(tester->IsActive(view2));

  panel->OnMouseMotion(gfx::Point(500, 500));
  EXPECT_FALSE(tester->IsActive(view1));
  EXPECT_FALSE(tester->IsActive(view2));
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestCloseDismissAllNonSticky) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  NotificationPanel* panel = GetNotificationPanel();
  NotificationPanelTester* tester = panel->GetTester();
  Profile* profile = browser()->profile();

  collection->Add(NewMockNotification("1"), profile);
  collection->AddSystemNotification(
      NewMockNotification("2"), profile, true, false);
  collection->Add(NewMockNotification("3"), profile);

  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(NotificationPanel::STICKY_AND_NEW, tester->state());
  EXPECT_EQ(3, tester->GetNotificationCount());
  EXPECT_EQ(1, tester->GetStickyNotificationCount());

  // Hide
  panel->Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, tester->GetNotificationCount());
  EXPECT_EQ(1, tester->GetStickyNotificationCount());
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestAddDOMUIMessageCallback) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  Profile* profile = browser()->profile();

  collection->AddSystemNotification(
      NewMockNotification("1"), profile, false, false);

  EXPECT_TRUE(collection->AddDOMUIMessageCallback(
      NewMockNotification("1"),
      "test",
      NewCallback(
          static_cast<NotificationTest*>(this),
          &NotificationTest::HandleDOMUIMessage)));

  // Adding callback for the same message twice should fail.
  EXPECT_FALSE(collection->AddDOMUIMessageCallback(
      NewMockNotification("1"),
      "test",
      NewCallback(
          static_cast<NotificationTest*>(this),
          &NotificationTest::HandleDOMUIMessage)));

  // Adding callback to nonexistent notification should fail.
  EXPECT_FALSE(collection->AddDOMUIMessageCallback(
      NewMockNotification("2"),
      "test1",
      NewCallback(
          static_cast<NotificationTest*>(this),
          &NotificationTest::HandleDOMUIMessage)));
}

IN_PROC_BROWSER_TEST_F(NotificationTest, TestDOMUIMessageCallback) {
  BalloonCollectionImpl* collection = GetBalloonCollectionImpl();
  Profile* profile = browser()->profile();
  // a notification that sends 'test' domui message back to chrome.
  const GURL content_url(
      "data:text/html;charset=utf-8,"
      "<html><script>function send() { chrome.send('test', ['']); }</script>"
      "<body onload='send()'></body></html>");
  collection->AddSystemNotification(
      Notification(GURL(), content_url, string16(), string16(),
                   new MockNotificationDelegate("1")),
      profile,
      false,
      false);
  EXPECT_TRUE(collection->AddDOMUIMessageCallback(
      NewMockNotification("1"),
      "test",
      NewCallback(
          static_cast<NotificationTest*>(this),
          &NotificationTest::HandleDOMUIMessage)));
  MessageLoop::current()->Run();
}

}  // namespace chromeos
