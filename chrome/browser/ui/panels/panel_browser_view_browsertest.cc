// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"

// BasePanelBrowserTest now creates refactored Panels. Refactor
// has only been done for Mac panels so far.
#if 0

class PanelBrowserViewTest : public BasePanelBrowserTest {
 public:
  PanelBrowserViewTest() : BasePanelBrowserTest() { }

 protected:
  PanelBrowserView* GetBrowserView(Panel* panel) const {
    return static_cast<PanelBrowserView*>(panel->native_panel());
  }

  gfx::Rect GetViewBounds(Panel* panel) const {
    return GetBrowserView(panel)->GetBounds();
  }

  void SetViewBounds(Panel* panel, const gfx::Rect& rect) const {
    return GetBrowserView(panel)->SetPanelBounds(rect);
  }

  gfx::NativeWindow GetNativeWindow(Panel* panel) const {
    return GetBrowserView(panel)->GetNativeWindow();
  }

  PanelBoundsAnimation* GetBoundsAnimator(Panel* panel) const {
    return GetBrowserView(panel)->bounds_animator_.get();
  }

  int GetTitlebarHeight(Panel* panel) const {
    PanelBrowserFrameView* frame_view = GetBrowserView(panel)->GetFrameView();
    return frame_view->NonClientTopBorderHeight() -
           frame_view->NonClientBorderThickness();
  }

  PanelBrowserFrameView::PaintState GetTitlebarPaintState(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->paint_state_;
  }

  bool IsTitlebarPaintedAsActive(Panel* panel) const {
    return GetTitlebarPaintState(panel) ==
           PanelBrowserFrameView::PAINT_AS_ACTIVE;
  }

  bool IsTitlebarPaintedAsInactive(Panel* panel) const {
    return GetTitlebarPaintState(panel) ==
           PanelBrowserFrameView::PAINT_AS_INACTIVE;
  }

  bool IsTitlebarPaintedForAttention(Panel* panel) const {
    return GetTitlebarPaintState(panel) ==
           PanelBrowserFrameView::PAINT_FOR_ATTENTION;
  }

  int GetControlCount(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->child_count();
  }

  TabIconView* GetTitleIcon(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->title_icon_;
  }

  views::Label* GetTitleText(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->title_label_;
  }

  views::Button* GetCloseButton(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->close_button_;
  }

  views::Button* GetMinimizeButton(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->minimize_button_;
  }

  views::Button* GetRestoreButton(Panel* panel) const {
    return GetBrowserView(panel)->GetFrameView()->restore_button_;
  }

  bool ContainsControl(Panel* panel, views::View* control) const {
    return GetBrowserView(panel)->GetFrameView()->Contains(control);
  }

  void WaitTillBoundsAnimationFinished(Panel* panel) {
    // The timer for the animation will only kick in as async task.
    while (GetBoundsAnimator(panel)->is_animating()) {
      MessageLoopForUI::current()->PostTask(FROM_HERE,
                                            MessageLoop::QuitClosure());
      MessageLoopForUI::current()->RunAllPending();
    }
  }

  void ClosePanelAndWaitForNotification(Panel* panel) {
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CLOSED,
        content::Source<Panel>(panel));
    panel->Close();
    signal.Wait();
  }

  // We put all the testing logic in this class instead of the test so that
  // we do not need to declare each new test as a friend of PanelBrowserView
  // for the purpose of accessing its private members.
  void TestMinimizeAndRestore(bool enable_auto_hiding) {
    PanelManager* panel_manager = PanelManager::GetInstance();
    DockedPanelStrip* docked_strip = panel_manager->docked_strip();
    int expected_bottom_on_expanded = docked_strip->display_area().bottom();
    int expected_bottom_on_title_only = expected_bottom_on_expanded;
    int expected_bottom_on_minimized = expected_bottom_on_expanded;

    // Turn on auto-hiding if requested.
    static const int bottom_thickness = 40;
    mock_display_settings_provider()->EnableAutoHidingDesktopBar(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        enable_auto_hiding,
        bottom_thickness);
    if (enable_auto_hiding)
      expected_bottom_on_title_only -= bottom_thickness;

    // Create and test one panel first.
    Panel* panel1 = CreatePanel("PanelTest1");
    PanelBrowserView* browser_view1 = GetBrowserView(panel1);
    PanelBrowserFrameView* frame_view1 = browser_view1->GetFrameView();

    // Test minimizing/restoring an individual panel.
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    int initial_height = panel1->GetBounds().height();

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());

    int titlebar_height = frame_view1->NonClientTopBorderHeight();
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);
    EXPECT_EQ(expected_bottom_on_minimized, panel1->GetBounds().bottom());
    WaitTillBoundsAnimationFinished(panel1);
    EXPECT_FALSE(panel1->IsActive());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_title_only, panel1->GetBounds().bottom());
    WaitTillBoundsAnimationFinished(panel1);
    EXPECT_TRUE(frame_view1->close_button_->visible());
    EXPECT_TRUE(frame_view1->title_icon_->visible());
    EXPECT_TRUE(frame_view1->title_label_->visible());

    panel1->SetExpansionState(Panel::EXPANDED);
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    EXPECT_EQ(initial_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_expanded, panel1->GetBounds().bottom());
    WaitTillBoundsAnimationFinished(panel1);
    EXPECT_TRUE(frame_view1->close_button_->visible());
    EXPECT_TRUE(frame_view1->title_icon_->visible());
    EXPECT_TRUE(frame_view1->title_label_->visible());

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);
    EXPECT_EQ(expected_bottom_on_minimized, panel1->GetBounds().bottom());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());
    EXPECT_EQ(expected_bottom_on_title_only, panel1->GetBounds().bottom());

    // Create 2 more panels for more testing.
    Panel* panel2 = CreatePanel("PanelTest2");
    Panel* panel3 = CreatePanel("PanelTest3");

    // Test bringing up or down the title-bar of all minimized panels.
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    panel3->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());

    mock_display_settings_provider()->SetDesktopBarVisibility(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        DisplaySettingsProvider::DESKTOP_BAR_VISIBLE);
    panel_manager->BringUpOrDownTitlebars(true);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::TITLE_ONLY, panel3->expansion_state());

    mock_display_settings_provider()->SetDesktopBarVisibility(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        DisplaySettingsProvider::DESKTOP_BAR_HIDDEN);
    panel_manager->BringUpOrDownTitlebars(false);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());

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
    EXPECT_FALSE(panel_manager->ShouldBringUpTitlebars(
        0, 0));
    browser_view1->OnTitlebarMousePressed(panel1->GetBounds().origin());
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    browser_view1->OnTitlebarMouseDragged(
        panel1->GetBounds().origin().Subtract(gfx::Point(5, 5)));
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_TRUE(panel_manager->ShouldBringUpTitlebars(
        0, 0));
    browser_view1->OnTitlebarMouseReleased(panel::NO_MODIFIER);

    ClosePanelAndWaitForNotification(panel1);
    ClosePanelAndWaitForNotification(panel2);
    ClosePanelAndWaitForNotification(panel3);
  }

  void TestChangeAutoHideTaskBarThickness() {
    PanelManager* manager = PanelManager::GetInstance();
    DockedPanelStrip* docked_strip = manager->docked_strip();
    int initial_starting_right_position = docked_strip->StartingRightPosition();

    int bottom_bar_thickness = 20;
    int right_bar_thickness = 30;
    mock_display_settings_provider()->EnableAutoHidingDesktopBar(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        true,
        bottom_bar_thickness);
    mock_display_settings_provider()->EnableAutoHidingDesktopBar(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        true,
        right_bar_thickness);
    EXPECT_EQ(
        initial_starting_right_position - docked_strip->StartingRightPosition(),
        right_bar_thickness);

    Panel* panel = CreatePanel("PanelTest");
    panel->SetExpansionState(Panel::TITLE_ONLY);
    WaitTillBoundsAnimationFinished(panel);

    EXPECT_EQ(docked_strip->display_area().bottom() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(docked_strip->StartingRightPosition(),
              panel->GetBounds().right());

    initial_starting_right_position = docked_strip->StartingRightPosition();
    int bottom_bar_thickness_delta = 10;
    bottom_bar_thickness += bottom_bar_thickness_delta;
    int right_bar_thickness_delta = 15;
    right_bar_thickness += right_bar_thickness_delta;
    mock_display_settings_provider()->SetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        bottom_bar_thickness);
    mock_display_settings_provider()->SetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        right_bar_thickness);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(
        initial_starting_right_position - docked_strip->StartingRightPosition(),
        right_bar_thickness_delta);
    EXPECT_EQ(docked_strip->display_area().bottom() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(docked_strip->StartingRightPosition(),
              panel->GetBounds().right());

    initial_starting_right_position = docked_strip->StartingRightPosition();
    bottom_bar_thickness_delta = 20;
    bottom_bar_thickness -= bottom_bar_thickness_delta;
    right_bar_thickness_delta = 10;
    right_bar_thickness -= right_bar_thickness_delta;
    mock_display_settings_provider()->SetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        bottom_bar_thickness);
    mock_display_settings_provider()->SetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        right_bar_thickness);
    MessageLoopForUI::current()->RunAllPending();
    EXPECT_EQ(
        docked_strip->StartingRightPosition() - initial_starting_right_position,
        right_bar_thickness_delta);
    EXPECT_EQ(docked_strip->display_area().bottom() - bottom_bar_thickness,
              panel->GetBounds().bottom());
    EXPECT_EQ(docked_strip->StartingRightPosition(),
              panel->GetBounds().right());

    panel->Close();
  }
};

// Panel is not supported for Linux view yet.
#if !defined(OS_LINUX) || !defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanelBasic) {
  CreatePanelParams params(
      "PanelTest", gfx::Rect(0, 0, 200, 150), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);

  // Validate basic window properties.
#if defined(OS_WIN) && !defined(USE_AURA)
  HWND native_window = GetNativeWindow(panel);

  RECT window_rect;
  EXPECT_TRUE(::GetWindowRect(native_window, &window_rect));
  EXPECT_EQ(200, window_rect.right - window_rect.left);
  EXPECT_EQ(150, window_rect.bottom - window_rect.top);

  EXPECT_TRUE(::IsWindowVisible(native_window));
#endif

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanelActive) {
  CreatePanelParams params("PanelTest", gfx::Rect(), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);

  // Validate it is active.
  EXPECT_TRUE(panel->IsActive());
  EXPECT_TRUE(IsTitlebarPaintedAsActive(panel));

  // Validate window styles. We want to ensure that the window is created
  // with expected styles regardless of its active state.
#if defined(OS_WIN) && !defined(USE_AURA)
  HWND native_window = GetNativeWindow(panel);

  LONG styles = ::GetWindowLong(native_window, GWL_STYLE);
  EXPECT_EQ(0, styles & WS_MAXIMIZEBOX);
  EXPECT_EQ(0, styles & WS_MINIMIZEBOX);

  LONG ext_styles = ::GetWindowLong(native_window, GWL_EXSTYLE);
  EXPECT_EQ(WS_EX_TOPMOST, ext_styles & WS_EX_TOPMOST);
#endif

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanelInactive) {
  CreatePanelParams params("PanelTest", gfx::Rect(), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);

  // Validate it is inactive.
  EXPECT_FALSE(panel->IsActive());
  EXPECT_FALSE(IsTitlebarPaintedAsActive(panel));

  // Validate window styles. We want to ensure that the window is created
  // with expected styles regardless of its active state.
#if defined(OS_WIN) && !defined(USE_AURA)
  HWND native_window = GetNativeWindow(panel);

  LONG styles = ::GetWindowLong(native_window, GWL_STYLE);
  EXPECT_EQ(0, styles & WS_MAXIMIZEBOX);
  EXPECT_EQ(0, styles & WS_MINIMIZEBOX);

  LONG ext_styles = ::GetWindowLong(native_window, GWL_EXSTYLE);
  EXPECT_EQ(WS_EX_TOPMOST, ext_styles & WS_EX_TOPMOST);
#endif

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, PanelLayout) {
  // Create a fixed-size panel to avoid possible collapsing of the title
  // if the enforced min sizes are too small.
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 200, 50));

  views::View* title_icon = GetTitleIcon(panel);
  views::View* title_text = GetTitleText(panel);
  views::View* close_button = GetCloseButton(panel);
  views::View* minimize_button = GetMinimizeButton(panel);
  views::View* restore_button = GetRestoreButton(panel);

  // We should have icon, text, minimize, restore and close buttons. Only one of
  // minimize and restore buttons are visible.
  EXPECT_EQ(5, GetControlCount(panel));
  EXPECT_TRUE(ContainsControl(panel, title_icon));
  EXPECT_TRUE(ContainsControl(panel, title_text));
  EXPECT_TRUE(ContainsControl(panel, close_button));
  EXPECT_TRUE(ContainsControl(panel, minimize_button));
  EXPECT_TRUE(ContainsControl(panel, restore_button));

  // These controls should be visible.
  EXPECT_TRUE(title_icon->visible());
  EXPECT_TRUE(title_text->visible());
  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(minimize_button->visible());
  EXPECT_FALSE(restore_button->visible());

  // Validate their layouts.
  int titlebar_height = GetTitlebarHeight(panel);
  EXPECT_GT(title_icon->width(), 0);
  EXPECT_GT(title_icon->height(), 0);
  EXPECT_LT(title_icon->height(), titlebar_height);
  EXPECT_GT(title_text->width(), 0);
  EXPECT_GT(title_text->height(), 0);
  EXPECT_LT(title_text->height(), titlebar_height);
  EXPECT_GT(minimize_button->width(), 0);
  EXPECT_GT(minimize_button->height(), 0);
  EXPECT_LT(minimize_button->height(), titlebar_height);
  EXPECT_GT(close_button->width(), 0);
  EXPECT_GT(close_button->height(), 0);
  EXPECT_LT(close_button->height(), titlebar_height);
  EXPECT_LT(title_icon->x() + title_icon->width(), title_text->x());
  EXPECT_LT(title_text->x() + title_text->width(), minimize_button->x());
  EXPECT_LT(minimize_button->x() + minimize_button->width(), close_button->x());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, SetBoundsAnimation) {
  Panel* panel = CreatePanel("PanelTest");
  PanelBrowserView* browser_view = GetBrowserView(panel);

  // The bounds animation should not be triggered when the panel is up for the
  // first time.
  EXPECT_FALSE(GetBoundsAnimator(panel));

  // Validate that animation should be triggered when bounds are changed.
  gfx::Rect target_bounds(GetViewBounds(panel));
  target_bounds.Offset(20, -30);
  target_bounds.set_width(target_bounds.width() + 100);
  target_bounds.set_height(target_bounds.height() + 50);
  SetViewBounds(panel, target_bounds);
  ASSERT_TRUE(GetBoundsAnimator(panel));
  EXPECT_TRUE(GetBoundsAnimator(panel)->is_animating());
  EXPECT_NE(GetViewBounds(panel), target_bounds);
  WaitTillBoundsAnimationFinished(panel);
  EXPECT_EQ(GetViewBounds(panel), target_bounds);

  // Validates that no animation should be triggered for the panel currently
  // being dragged.
  browser_view->OnTitlebarMousePressed(gfx::Point(
      target_bounds.x(), target_bounds.y()));
  browser_view->OnTitlebarMouseDragged(gfx::Point(
      target_bounds.x() + 5, target_bounds.y() + 5));
  EXPECT_FALSE(GetBoundsAnimator(panel)->is_animating());
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

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest,
                       ChangeAutoHideTaskBarThickness) {
  TestChangeAutoHideTaskBarThickness();
}
#endif

#endif  // #if 0 - until Panel refactored for Views
