// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection_impl.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/show_desktop_notification_params.h"
#include "ui/gfx/screen.h"

// Desktop notification code subscribes to various panel change notifications
// so that it knows when to adjusts balloon positions. In order to give
// desktop notification code a chance to process the change notifications,
// we call MessageLoopForUI::current()->RunUntilIdle() after any panel change
// has been made.
class PanelAndDesktopNotificationTest : public BasePanelBrowserTest {
 public:
  PanelAndDesktopNotificationTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelAndDesktopNotificationTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Do not use our own testing work area since desktop notification code
    // does not have the hook up for testing work area.
    disable_display_settings_mock();

    BasePanelBrowserTest::SetUpOnMainThread();

    g_browser_process->local_state()->SetInteger(
        prefs::kDesktopNotificationPosition, BalloonCollection::LOWER_RIGHT);
    ui_manager_.reset(new BalloonNotificationUIManager(
        g_browser_process->local_state()));
    ui_manager_->SetBalloonCollection(BalloonCollection::Create());
    service_.reset(new DesktopNotificationService(browser()->profile(),
                   ui_manager_.get()));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    ui_manager_->balloon_collection()->RemoveAll();
    MessageLoopForUI::current()->RunUntilIdle();

    service_.reset();
    ui_manager_.reset();

    BasePanelBrowserTest::CleanUpOnMainThread();
  }

  content::ShowDesktopNotificationHostMsgParams StandardTestNotification() {
    content::ShowDesktopNotificationHostMsgParams params;
    params.notification_id = 0;
    params.origin = GURL("http://www.google.com");
    params.is_html = false;
    params.icon_url = GURL("/icon.png");
    params.title = ASCIIToUTF16("Title");
    params.body = ASCIIToUTF16("Text");
    params.direction = WebKit::WebTextDirectionDefault;
    return params;
  }

  Balloon* CreateBalloon() {
    content::ShowDesktopNotificationHostMsgParams params =
        StandardTestNotification();
    EXPECT_TRUE(service()->ShowDesktopNotification(
          params, 0, 0, DesktopNotificationService::PageNotification));
    MessageLoopForUI::current()->RunUntilIdle();
    return balloons().front();
  }

  static int GetBalloonBottomPosition(Balloon* balloon) {
#if defined(OS_MACOSX)
    // The position returned by the notification balloon is based on Mac's
    // vertically inverted orientation. We need to flip it so that it can
    // be compared against the position returned by the panel.
    gfx::Size screen_size =
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().size();
    return screen_size.height() - balloon->GetPosition().y();
#else
    return balloon->GetPosition().y() + balloon->GetViewSize().height();
#endif
  }

  static void DragPanelToMouseLocation(Panel* panel,
                                       const gfx::Point& new_mouse_location) {
    PanelManager* panel_manager = PanelManager::GetInstance();
    panel_manager->StartDragging(panel, panel->GetBounds().origin());
    panel_manager->Drag(new_mouse_location);
    panel_manager->EndDragging(false);
  }

  static void ResizePanelByMouseWithDelta(Panel* panel,
                                          panel::ResizingSides side,
                                          const gfx::Vector2d& delta) {
    PanelManager* panel_manager = PanelManager::GetInstance();
    gfx::Point mouse_location = panel->GetBounds().origin();
    panel_manager->StartResizingByMouse(panel, mouse_location, side);
    panel_manager->ResizeByMouse(mouse_location + delta);
    panel_manager->EndResizingByMouse(false);
  }

  DesktopNotificationService* service() const { return service_.get(); }
  const BalloonCollection::Balloons& balloons() const {
    return ui_manager_->balloon_collection()->GetActiveBalloons();
  }

 private:
  scoped_ptr<BalloonNotificationUIManager> ui_manager_;
  scoped_ptr<DesktopNotificationService> service_;
};

// As filed in http://crbug.com/178292.
IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest,
                       DISABLED_AddAndClosePanel) {
  Balloon* balloon = CreateBalloon();
  int original_balloon_bottom = GetBalloonBottomPosition(balloon);

  // Create a docked panel. Expect that the notification balloon moves up to be
  // above the panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom, panel->GetBounds().y());
  EXPECT_LT(balloon_bottom, original_balloon_bottom);

  // Close the panel. Expect the notification balloon moves back to its original
  // position.
  panel->Close();
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(original_balloon_bottom, GetBalloonBottomPosition(balloon));
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest,
                       ExpandAndCollapsePanel) {
  Balloon* balloon = CreateBalloon();

  // Create a docked panel. Expect that the notification balloon moves up to be
  // above the panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_on_expanded = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_on_expanded, panel->GetBounds().y());

  // Minimize the panel. Expect that the notification balloon moves down, but
  // still above the minimized panel.
  panel->Minimize();
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_on_minimized = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_on_minimized, panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_on_expanded, balloon_bottom_on_minimized);

  // Bring up the title-bar for the panel by drawing attention. Expect that the
  // notification balloon moves up a little bit to be still above the title-only
  // panel.
  panel->FlashFrame(true);
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_on_title_only = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_on_title_only, panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_on_title_only, balloon_bottom_on_minimized);
  EXPECT_LT(balloon_bottom_on_expanded, balloon_bottom_on_title_only);

  // Expand the panel. Expect that the notification balloon moves up to go back
  // to the same position when the panel is expanded.
  panel->Restore();
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(balloon_bottom_on_expanded, GetBalloonBottomPosition(balloon));

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, DragNarrowPanel) {
  Balloon* balloon = CreateBalloon();

  // Let the panel width be smaller than the balloon width.
  int panel_width = balloon->GetViewSize().width() - 50;

  // Create 2 docked panels. Expect that the notification balloon moves up to be
  // above the tall panel.
  Panel* tall_panel = CreateDockedPanel("1", gfx::Rect(0, 0, panel_width, 300));
  Panel* short_panel = CreateDockedPanel(
      "2", gfx::Rect(0, 0, panel_width, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom, tall_panel->GetBounds().y());

  // Swap 2 docked panels by dragging. Expect that the notificaition balloon
  // remains at the same position.
  DragPanelToMouseLocation(tall_panel, short_panel->GetBounds().origin());
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(balloon_bottom, GetBalloonBottomPosition(balloon));

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, DragWidePanel) {
  Balloon* balloon = CreateBalloon();

  // Let the panel width be greater than the balloon width.
  int panel_width = balloon->GetViewSize().width() + 50;

  // Create 2 docked panels. Expect that the notification balloon moves up to be
  // above the tall panel.
  Panel* tall_panel = CreateDockedPanel("1", gfx::Rect(0, 0, panel_width, 300));
  Panel* short_panel = CreateDockedPanel(
      "2", gfx::Rect(0, 0, panel_width, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_before_drag = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_before_drag, tall_panel->GetBounds().y());

  // Swap 2 docked panels by dragging. Expect that the notificaiton balloon
  // moves down to be just above the short panel.
  DragPanelToMouseLocation(tall_panel, short_panel->GetBounds().origin());
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_drag = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_drag, short_panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_before_drag, balloon_bottom_after_drag);

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, DetachAndAttachPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Balloon* balloon = CreateBalloon();
  int original_balloon_bottom = GetBalloonBottomPosition(balloon);

  // Create a docked panel. Expect that the notification balloon moves up to be
  // above the panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_panel_created = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_panel_created, panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_panel_created, original_balloon_bottom);

  // Detach the panel. Expect that the notification balloon moves down to its
  // original position.
  panel_manager->MovePanelToCollection(panel,
                                       panel_manager->detached_collection(),
                                       PanelCollection::DEFAULT_POSITION);
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  EXPECT_EQ(original_balloon_bottom, GetBalloonBottomPosition(balloon));

  // Reattach the panel. Expect that the notification balloon moves above the
  // panel.
  panel_manager->MovePanelToCollection(panel,
                                       panel_manager->docked_collection(),
                                       PanelCollection::DEFAULT_POSITION);
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  EXPECT_EQ(balloon_bottom_after_panel_created,
            GetBalloonBottomPosition(balloon));

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, ResizePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Balloon* balloon = CreateBalloon();

  // Create a docked panel. Expect that the notification balloon moves up to be
  // above the panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom = GetBalloonBottomPosition(balloon);
  gfx::Rect original_bounds = panel->GetBounds();
  EXPECT_LT(balloon_bottom, original_bounds.y());

  // Resize the panel to make it taller. Expect that the notification balloon
  // moves further up by the amount of enlarge offset.
  gfx::Point resize_delta(50, 100);
  gfx::Rect new_bounds = original_bounds;
  new_bounds.set_width(new_bounds.width() + resize_delta.x());
  new_bounds.set_height(new_bounds.height() + resize_delta.y());
  panel->SetBounds(new_bounds);
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom2 = GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom - resize_delta.y(), balloon_bottom2);

  // Resize the panel to make it shorter. Expect that the notification balloon
  // moves down by the amount of shrink offset.
  resize_delta = gfx::Point(0, -60);
  new_bounds.set_width(new_bounds.width() + resize_delta.x());
  new_bounds.set_height(new_bounds.height() + resize_delta.y());
  panel->SetBounds(new_bounds);
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom3 = GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom2 - resize_delta.y(), balloon_bottom3);

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, ResizePanelByMouse) {
  Balloon* balloon = CreateBalloon();

  // Create a docked panel. Expect that the notification balloon moves up to be
  // above the panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom = GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom, panel->GetBounds().y());

  // Resize the panel to make it taller. Expect that the notification balloon
  // moves further up by the amount of enlarge offset.
  gfx::Vector2d drag_delta(-50, -100);
  ResizePanelByMouseWithDelta(panel, panel::RESIZE_TOP_LEFT, drag_delta);
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom2 = GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom + drag_delta.y(), balloon_bottom2);

  // Resize the panel to make it shorter. Expect that the notification balloon
  // moves down by the amount of shrink offset.
  drag_delta = gfx::Vector2d(0, 60);
  ResizePanelByMouseWithDelta(panel, panel::RESIZE_TOP, drag_delta);
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom3 = GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom2 + drag_delta.y(), balloon_bottom3);

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelAndDesktopNotificationTest, InteractWithTwoPanels) {
  Balloon* balloon = CreateBalloon();
  int original_balloon_bottom = GetBalloonBottomPosition(balloon);

  // Let the panel width be smaller than the balloon width.
  int panel_width = balloon->GetViewSize().width() - 50;

  // Create a short panel. Expect that the notification balloon moves up to be
  // above the short panel.
  Panel* short_panel = CreateDockedPanel(
      "1", gfx::Rect(0, 0, panel_width, 150));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_short_panel_created =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_short_panel_created,
            short_panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_short_panel_created, original_balloon_bottom);

  // Create a tall panel. Expect that the notification balloon moves further up
  // to be above the tall panel.
  Panel* tall_panel = CreateDockedPanel("2", gfx::Rect(0, 0, panel_width, 200));
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_tall_panel_created =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_tall_panel_created,
            tall_panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_tall_panel_created,
            balloon_bottom_after_short_panel_created);

  // Minimize tall panel. Expect that the notification balloon moves down to the
  // same position when short panel is first created.
  tall_panel->Minimize();
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_tall_panel_minimized =
      GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_tall_panel_minimized);

  // Minimize short panel. Expect that the notification balloon moves further
  // down.
  short_panel->Minimize();
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_both_panels_minimized =
      GetBalloonBottomPosition(balloon);
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            short_panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            tall_panel->GetBounds().y());
  EXPECT_LT(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_both_panels_minimized);
  EXPECT_LT(balloon_bottom_after_both_panels_minimized,
            original_balloon_bottom);

  // Expand short panel. Expect that the notification balloon moves further up
  // to the same position when short panel is first created.
  short_panel->Restore();
  MessageLoopForUI::current()->RunUntilIdle();
  int balloon_bottom_after_short_panel_expanded =
      GetBalloonBottomPosition(balloon);
  EXPECT_EQ(balloon_bottom_after_short_panel_created,
            balloon_bottom_after_short_panel_expanded);

  // Close tall panel. Expect that the notification balloon moves down to the
  // same position when short panel is first created.
  tall_panel->Close();
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(balloon_bottom_after_short_panel_created,
            GetBalloonBottomPosition(balloon));

  // Close short panel. Expect that the notification balloo moves back to its
  // original position.
  short_panel->Close();
  MessageLoopForUI::current()->RunUntilIdle();
  EXPECT_EQ(original_balloon_bottom, GetBalloonBottomPosition(balloon));
}
