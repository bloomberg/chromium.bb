// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/slide_animation.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"

class PanelBrowserViewTest : public BasePanelBrowserTest {
 public:
  PanelBrowserViewTest() : BasePanelBrowserTest() { }

 protected:
  class MockMouseWatcher : public PanelBrowserFrameView::MouseWatcher {
   public:
    explicit MockMouseWatcher(PanelBrowserFrameView* view)
        : PanelBrowserFrameView::MouseWatcher(view),
          is_cursor_in_view_(false) {
    }

    virtual bool IsCursorInViewBounds() const {
      return is_cursor_in_view_;
    }

    void MoveMouse(bool is_cursor_in_view) {
      is_cursor_in_view_ = is_cursor_in_view;

#if defined(OS_WIN)
      MSG msg;
      msg.message = WM_MOUSEMOVE;
      DidProcessMessage(msg);
#else
      GdkEvent event;
      event.type = GDK_MOTION_NOTIFY;
      DidProcessEvent(&event);
#endif
    }

   private:
    bool is_cursor_in_view_;
  };

  PanelBrowserView* GetBrowserView(Panel* panel) {
    return static_cast<PanelBrowserView*>(panel->native_panel());
  }

  void WaitTillBoundsAnimationFinished(PanelBrowserView* browser_view) {
    // The timer for the animation will only kick in as async task.
    while (browser_view->bounds_animator_->is_animating()) {
      MessageLoopForUI::current()->PostTask(FROM_HERE,
                                            new MessageLoop::QuitTask());
      MessageLoopForUI::current()->RunAllPending();
    }
  }

  void WaitTillSettingsAnimationFinished(PanelBrowserFrameView* frame_view) {
    // The timer for the animation will only kick in as async task.
    while (frame_view->settings_button_animator_->is_animating()) {
      MessageLoopForUI::current()->PostTask(FROM_HERE,
                                            new MessageLoop::QuitTask());
      MessageLoopForUI::current()->RunAllPending();
    }
  }

  void TestShowPanelActiveOrInactive() {
    CreatePanelParams params1("PanelTest1", gfx::Rect(), SHOW_AS_ACTIVE);
    Panel* panel1 = CreatePanelWithParams(params1);
    PanelBrowserView* browser_view1 = GetBrowserView(panel1);
    PanelBrowserFrameView* frame_view1 = browser_view1->GetFrameView();
    EXPECT_TRUE(panel1->IsActive());
    EXPECT_EQ(PanelBrowserFrameView::PAINT_AS_ACTIVE,
              frame_view1->paint_state_);

    CreatePanelParams params2("PanelTest2", gfx::Rect(), SHOW_AS_INACTIVE);
    Panel* panel2 = CreatePanelWithParams(params2);
    PanelBrowserView* browser_view2 = GetBrowserView(panel2);
    PanelBrowserFrameView* frame_view2 = browser_view2->GetFrameView();
    EXPECT_FALSE(panel2->IsActive());
    EXPECT_EQ(PanelBrowserFrameView::PAINT_AS_INACTIVE,
              frame_view2->paint_state_);

    panel1->Close();
    panel2->Close();
  }

  // We put all the testing logic in this class instead of the test so that
  // we do not need to declare each new test as a friend of PanelBrowserView
  // for the purpose of accessing its private members.
  void TestMinimizeAndRestore(bool enable_auto_hiding) {
    PanelManager* panel_manager = PanelManager::GetInstance();
    int expected_bottom_on_minimized = testing_work_area().height();
    int expected_bottom_on_unminimized = expected_bottom_on_minimized;

    // Turn on auto-hiding if requested.
    static const int bottom_thickness = 40;
    mock_auto_hiding_desktop_bar()->EnableAutoHiding(
        AutoHidingDesktopBar::ALIGN_BOTTOM,
        enable_auto_hiding,
        bottom_thickness);
    if (enable_auto_hiding)
      expected_bottom_on_unminimized -= bottom_thickness;

    // Create and test one panel first.
    Panel* panel1 = CreatePanel("PanelTest1");
    PanelBrowserView* browser_view1 = GetBrowserView(panel1);
    PanelBrowserFrameView* frame_view1 = browser_view1->GetFrameView();

    // Test minimizing/restoring an individual panel.
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    EXPECT_EQ(0, panel_manager->minimized_panel_count());
    int initial_height = panel1->GetBounds().height();
    int titlebar_height = frame_view1->NonClientTopBorderHeight();

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);
    EXPECT_EQ(expected_bottom_on_minimized, panel1->GetBounds().bottom());
    EXPECT_EQ(1, panel_manager->minimized_panel_count());
    WaitTillBoundsAnimationFinished(browser_view1);
    EXPECT_FALSE(panel1->IsActive());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_unminimized, panel1->GetBounds().bottom());
    EXPECT_EQ(1, panel_manager->minimized_panel_count());
    WaitTillBoundsAnimationFinished(browser_view1);
    EXPECT_TRUE(frame_view1->close_button_->IsVisible());
    EXPECT_TRUE(frame_view1->title_icon_->IsVisible());
    EXPECT_TRUE(frame_view1->title_label_->IsVisible());

    panel1->SetExpansionState(Panel::EXPANDED);
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    EXPECT_EQ(initial_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_unminimized, panel1->GetBounds().bottom());
    EXPECT_EQ(0, panel_manager->minimized_panel_count());
    WaitTillBoundsAnimationFinished(browser_view1);
    EXPECT_TRUE(frame_view1->close_button_->IsVisible());
    EXPECT_TRUE(frame_view1->title_icon_->IsVisible());
    EXPECT_TRUE(frame_view1->title_label_->IsVisible());

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);
    EXPECT_EQ(expected_bottom_on_minimized, panel1->GetBounds().bottom());
    EXPECT_EQ(1, panel_manager->minimized_panel_count());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_unminimized, panel1->GetBounds().bottom());
    EXPECT_EQ(1, panel_manager->minimized_panel_count());

    // Create 2 more panels for more testing.
    Panel* panel2 = CreatePanel("PanelTest2");
    Panel* panel3 = CreatePanel("PanelTest3");

    // Test bringing up or down the title-bar of all minimized panels.
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    panel3->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());
    EXPECT_EQ(2, panel_manager->minimized_panel_count());

    mock_auto_hiding_desktop_bar()->SetVisibility(
        AutoHidingDesktopBar::ALIGN_BOTTOM, AutoHidingDesktopBar::VISIBLE);
    panel_manager->BringUpOrDownTitlebars(true);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::TITLE_ONLY, panel3->expansion_state());
    EXPECT_EQ(2, panel_manager->minimized_panel_count());

    mock_auto_hiding_desktop_bar()->SetVisibility(
        AutoHidingDesktopBar::ALIGN_BOTTOM, AutoHidingDesktopBar::HIDDEN);
    panel_manager->BringUpOrDownTitlebars(false);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());
    EXPECT_EQ(2, panel_manager->minimized_panel_count());

    // Test if it is OK to bring up title-bar given the mouse position.
    EXPECT_TRUE(panel_manager->ShouldBringUpTitlebars(
        panel1->GetBounds().x(), panel1->GetBounds().y()));
    EXPECT_FALSE(panel_manager->ShouldBringUpTitlebars(
        panel2->GetBounds().x(), panel2->GetBounds().y()));
    EXPECT_TRUE(panel_manager->ShouldBringUpTitlebars(
        panel3->GetBounds().right() - 1, panel3->GetBounds().bottom() - 1));
    EXPECT_TRUE(panel_manager->ShouldBringUpTitlebars(
        panel3->GetBounds().right() - 1, panel3->GetBounds().bottom() + 10));
    EXPECT_FALSE(panel_manager->ShouldBringUpTitlebars(
        0, 0));

    // Test that the panel in title-only state should not be minimized
    // regardless of the current mouse position when the panel is being dragged.
    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(2, panel_manager->minimized_panel_count());
    EXPECT_FALSE(panel_manager->ShouldBringUpTitlebars(
        0, 0));
    browser_view1->OnTitlebarMousePressed(panel1->GetBounds().origin());
    browser_view1->OnTitlebarMouseDragged(
        panel1->GetBounds().origin().Subtract(gfx::Point(5, 5)));
    EXPECT_TRUE(panel_manager->ShouldBringUpTitlebars(
        0, 0));
    browser_view1->OnTitlebarMouseReleased();

    panel1->Close();
    panel2->Close();
    panel3->Close();
    EXPECT_EQ(0, panel_manager->minimized_panel_count());
  }

  void TestDrawAttention() {
    Panel* panel = CreatePanel("PanelTest");
    PanelBrowserView* browser_view = GetBrowserView(panel);
    PanelBrowserFrameView* frame_view = browser_view->GetFrameView();
    SkColor attention_color = frame_view->GetTitleColor(
        PanelBrowserFrameView::PAINT_FOR_ATTENTION);

    // Test that the attention should not be drawn if the expanded panel is in
    // focus.
    browser_view->DrawAttention();
    EXPECT_FALSE(browser_view->IsDrawingAttention());
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_NE(attention_color, frame_view->title_label_->GetColor());

    // Test that the attention is drawn when the expanded panel is not in focus.
    panel->Deactivate();
    browser_view->DrawAttention();
    EXPECT_TRUE(browser_view->IsDrawingAttention());
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(attention_color, frame_view->title_label_->GetColor());

    // Test that the attention is cleared.
    browser_view->StopDrawingAttention();
    EXPECT_FALSE(browser_view->IsDrawingAttention());
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_NE(attention_color, frame_view->title_label_->GetColor());

    // Test that the attention is drawn and the title-bar is brought up when the
    // minimized panel is not in focus.
    panel->Deactivate();
    panel->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());
    browser_view->DrawAttention();
    EXPECT_TRUE(browser_view->IsDrawingAttention());
    EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(attention_color, frame_view->title_label_->GetColor());

    // Test that we cannot bring up other minimized panel if the mouse is over
    // the panel that draws attension.
    EXPECT_FALSE(PanelManager::GetInstance()->
        ShouldBringUpTitlebars(panel->GetBounds().x(), panel->GetBounds().y()));

    // Test that we cannot bring down the panel that is drawing the attention.
    PanelManager::GetInstance()->BringUpOrDownTitlebars(false);
    EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

    // Test that the attention is cleared.
    browser_view->StopDrawingAttention();
    EXPECT_FALSE(browser_view->IsDrawingAttention());
    EXPECT_EQ(Panel::EXPANDED, panel->expansion_state());
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_NE(attention_color, frame_view->title_label_->GetColor());

    panel->Close();
  }

  void TestChangeAutoHideTaskBarThickness() {
    int bottom_bar_thickness = 20;
    int right_bar_thickness = 30;
    mock_auto_hiding_desktop_bar()->EnableAutoHiding(
        AutoHidingDesktopBar::ALIGN_BOTTOM, true, bottom_bar_thickness);
    mock_auto_hiding_desktop_bar()->EnableAutoHiding(
        AutoHidingDesktopBar::ALIGN_RIGHT, true, right_bar_thickness);

    Panel* panel = CreatePanel("PanelTest");
    EXPECT_EQ(testing_work_area().height() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(testing_work_area().right() - right_bar_thickness,
              panel->GetBounds().right());

    bottom_bar_thickness += 10;
    right_bar_thickness += 15;
    mock_auto_hiding_desktop_bar()->SetThickness(
        AutoHidingDesktopBar::ALIGN_BOTTOM, bottom_bar_thickness);
    mock_auto_hiding_desktop_bar()->SetThickness(
        AutoHidingDesktopBar::ALIGN_RIGHT, right_bar_thickness);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(testing_work_area().height() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(testing_work_area().right() - right_bar_thickness,
              panel->GetBounds().right());

    bottom_bar_thickness -= 20;
    right_bar_thickness -= 10;
    mock_auto_hiding_desktop_bar()->SetThickness(
        AutoHidingDesktopBar::ALIGN_BOTTOM, bottom_bar_thickness);
    mock_auto_hiding_desktop_bar()->SetThickness(
        AutoHidingDesktopBar::ALIGN_RIGHT, right_bar_thickness);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(testing_work_area().height() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(testing_work_area().right() - right_bar_thickness,
              panel->GetBounds().right());

    panel->Close();
  }
};

// Panel is not supported for Linux view yet.
#if !defined(OS_LINUX) || !defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanel) {
  Panel* panel = CreatePanel("PanelTest");
  PanelBrowserView* browser_view = GetBrowserView(panel);
  PanelBrowserFrameView* frame_view = browser_view->GetFrameView();

  // The bounds animation should not be triggered when the panel is up for the
  // first time.
  EXPECT_FALSE(browser_view->bounds_animator_.get());

  // We should have icon, text, settings button and close button.
  EXPECT_EQ(4, frame_view->child_count());
  EXPECT_TRUE(frame_view->Contains(frame_view->title_icon_));
  EXPECT_TRUE(frame_view->Contains(frame_view->title_label_));
  EXPECT_TRUE(frame_view->Contains(frame_view->settings_button_));
  EXPECT_TRUE(frame_view->Contains(frame_view->close_button_));

  // These controls should be visible.
  EXPECT_TRUE(frame_view->title_icon_->IsVisible());
  EXPECT_TRUE(frame_view->title_label_->IsVisible());
  EXPECT_TRUE(frame_view->close_button_->IsVisible());

  // Validate their layouts.
  int titlebar_height = frame_view->NonClientTopBorderHeight() -
      frame_view->NonClientBorderThickness();
  EXPECT_GT(frame_view->title_icon_->width(), 0);
  EXPECT_GT(frame_view->title_icon_->height(), 0);
  EXPECT_LT(frame_view->title_icon_->height(), titlebar_height);
  EXPECT_GT(frame_view->title_label_->width(), 0);
  EXPECT_GT(frame_view->title_label_->height(), 0);
  EXPECT_LT(frame_view->title_label_->height(), titlebar_height);
  EXPECT_GT(frame_view->settings_button_->width(), 0);
  EXPECT_GT(frame_view->settings_button_->height(), 0);
  EXPECT_LT(frame_view->settings_button_->height(), titlebar_height);
  EXPECT_GT(frame_view->close_button_->width(), 0);
  EXPECT_GT(frame_view->close_button_->height(), 0);
  EXPECT_LT(frame_view->close_button_->height(), titlebar_height);
  EXPECT_LT(frame_view->title_icon_->x() + frame_view->title_icon_->width(),
      frame_view->title_label_->x());
  EXPECT_LT(frame_view->title_label_->x() + frame_view->title_label_->width(),
      frame_view->settings_button_->x());
  EXPECT_LT(
      frame_view->settings_button_->x() + frame_view->settings_button_->width(),
      frame_view->close_button_->x());

  // Validate that the controls should be updated when the activation state is
  // changed.
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_ACTIVE);
  SkColor title_label_color1 = frame_view->title_label_->GetColor();
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_INACTIVE);
  SkColor title_label_color2 = frame_view->title_label_->GetColor();
  EXPECT_NE(title_label_color1, title_label_color2);

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, ShowPanelActiveOrInactive) {
  TestShowPanelActiveOrInactive();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, ShowOrHideSettingsButton) {
  Panel* panel = CreatePanel("PanelTest");
  PanelBrowserFrameView* frame_view = GetBrowserView(panel)->GetFrameView();

  // Create and hook up the MockMouseWatcher so that we can simulate if the
  // mouse is over the panel.
  MockMouseWatcher* mouse_watcher = new MockMouseWatcher(frame_view);
  frame_view->set_mouse_watcher(mouse_watcher);

  // When the panel is created, it is active. Since we cannot programatically
  // bring the panel back to active state once it is deactivated, we have to
  // test the cases that the panel is active first.
  EXPECT_TRUE(panel->IsActive());

  // When the panel is active, the settings button should always be visible.
  mouse_watcher->MoveMouse(true);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());
  mouse_watcher->MoveMouse(false);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());

  // When the panel is inactive, the options button is active per the mouse over
  // the panel or not.
  panel->Deactivate();
  EXPECT_FALSE(panel->IsActive());
  WaitTillSettingsAnimationFinished(frame_view);
  EXPECT_FALSE(frame_view->settings_button_->IsVisible());

  mouse_watcher->MoveMouse(true);
  WaitTillSettingsAnimationFinished(frame_view);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());
  mouse_watcher->MoveMouse(false);
  WaitTillSettingsAnimationFinished(frame_view);
  EXPECT_FALSE(frame_view->settings_button_->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, SetBoundsAnimation) {
  Panel* panel = CreatePanel("PanelTest");
  PanelBrowserView* browser_view = GetBrowserView(panel);

  // Validate that animation should be triggered when bounds are changed.
  gfx::Rect target_bounds(browser_view->GetBounds());
  target_bounds.Offset(20, -30);
  target_bounds.set_width(target_bounds.width() + 100);
  target_bounds.set_height(target_bounds.height() + 50);
  browser_view->SetPanelBounds(target_bounds);
  ASSERT_TRUE(browser_view->bounds_animator_.get());
  EXPECT_TRUE(browser_view->bounds_animator_->is_animating());
  EXPECT_NE(browser_view->GetBounds(), target_bounds);
  WaitTillBoundsAnimationFinished(browser_view);
  EXPECT_EQ(browser_view->GetBounds(), target_bounds);

  // Validates that no animation should be triggered for the panel currently
  // being dragged.
  browser_view->OnTitlebarMousePressed(gfx::Point(
      target_bounds.x(), target_bounds.y()));
  browser_view->OnTitlebarMouseDragged(gfx::Point(
      target_bounds.x() + 5, target_bounds.y() + 5));
  EXPECT_FALSE(browser_view->bounds_animator_->is_animating());
  browser_view->OnTitlebarMouseCaptureLost();

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest,
                       MinimizeAndRestoreOnNormalTaskBar) {
  TestMinimizeAndRestore(false);
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest,
                       MinimizeAndRestoreOnAutoHideTaskBar) {
  TestMinimizeAndRestore(true);
}

// TODO(jianli): Investigate why this fails on win trunk build.
IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, DISABLED_DrawAttention) {
  TestDrawAttention();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, ChangeAutoHideTaskBarThickness) {
  TestChangeAutoHideTaskBarThickness();
}
#endif
