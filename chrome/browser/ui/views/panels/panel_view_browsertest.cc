// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/views/panels/panel_frame_view.h"
#include "chrome/browser/ui/views/panels/panel_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

// BasePanelBrowserTest now creates refactored Panels. Refactor
// has only been done for Mac panels so far.
class PanelViewTest : public BasePanelBrowserTest {
 public:
  PanelViewTest() : BasePanelBrowserTest() { }

 protected:
  PanelView* GetPanelView(Panel* panel) const {
    return static_cast<PanelView*>(panel->native_panel());
  }
};

IN_PROC_BROWSER_TEST_F(PanelViewTest, ActivePanelWindowProperties) {
  CreatePanelParams params("1", gfx::Rect(0, 0, 200, 150), SHOW_AS_ACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_TRUE(panel->IsActive());

  // Validate window styles. We want to ensure that the window is created
  // with expected styles regardless of its active state.
#if defined(OS_WIN)
  HWND native_window = views::HWNDForWidget(GetPanelView(panel)->window());

  LONG styles = ::GetWindowLong(native_window, GWL_STYLE);
  EXPECT_EQ(0, styles & WS_MAXIMIZEBOX);
  EXPECT_EQ(0, styles & WS_MINIMIZEBOX);

  LONG ext_styles = ::GetWindowLong(native_window, GWL_EXSTYLE);
  EXPECT_EQ(WS_EX_TOPMOST, ext_styles & WS_EX_TOPMOST);

  RECT window_rect;
  EXPECT_TRUE(::GetWindowRect(native_window, &window_rect));
  EXPECT_EQ(200, window_rect.right - window_rect.left);
  EXPECT_EQ(150, window_rect.bottom - window_rect.top);

  EXPECT_TRUE(::IsWindowVisible(native_window));
#endif

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelViewTest, InactivePanelWindowProperties) {
  CreatePanelParams params("1", gfx::Rect(0, 0, 200, 150), SHOW_AS_INACTIVE);
  Panel* panel = CreatePanelWithParams(params);
  EXPECT_FALSE(panel->IsActive());

  // Validate window styles. We want to ensure that the window is created
  // with expected styles regardless of its active state.
#if defined(OS_WIN)
  HWND native_window = views::HWNDForWidget(GetPanelView(panel)->window());

  LONG styles = ::GetWindowLong(native_window, GWL_STYLE);
  EXPECT_EQ(0, styles & WS_MAXIMIZEBOX);
  EXPECT_EQ(0, styles & WS_MINIMIZEBOX);

  LONG ext_styles = ::GetWindowLong(native_window, GWL_EXSTYLE);
  EXPECT_EQ(WS_EX_TOPMOST, ext_styles & WS_EX_TOPMOST);

  RECT window_rect;
  EXPECT_TRUE(::GetWindowRect(native_window, &window_rect));
  EXPECT_EQ(200, window_rect.right - window_rect.left);
  EXPECT_EQ(150, window_rect.bottom - window_rect.top);

  EXPECT_TRUE(::IsWindowVisible(native_window));
#endif

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelViewTest, PanelLayout) {
  // Create a fixed-size panel to avoid possible collapsing of the title
  // if the enforced min sizes are too small.
  Panel* panel = CreatePanelWithBounds("PanelTest", gfx::Rect(0, 0, 200, 50));
  PanelFrameView* frame_view = GetPanelView(panel)->GetFrameView();

  TabIconView* title_icon = frame_view->title_icon();
  views::View* title_text = frame_view->title_label();
  views::View* close_button = frame_view->close_button();
  views::View* minimize_button = frame_view->minimize_button();
  views::View* restore_button = frame_view->restore_button();

  // We should have icon, text, minimize, restore and close buttons. Only one of
  // minimize and restore buttons are visible.
  EXPECT_EQ(5, frame_view->child_count());
  EXPECT_TRUE(frame_view->Contains(title_icon));
  EXPECT_TRUE(frame_view->Contains(title_text));
  EXPECT_TRUE(frame_view->Contains(close_button));
  EXPECT_TRUE(frame_view->Contains(minimize_button));
  EXPECT_TRUE(frame_view->Contains(restore_button));

  // These controls should be visible.
  EXPECT_TRUE(title_icon->visible());
  EXPECT_TRUE(title_text->visible());
  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(minimize_button->visible());
  EXPECT_FALSE(restore_button->visible());

  // Validate their layouts.
  int titlebar_height = panel::kTitlebarHeight;
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

IN_PROC_BROWSER_TEST_F(PanelViewTest, CheckTitleOnlyHeight) {
  gfx::Rect bounds(0, 0, 200, 50);
  Panel* panel = CreatePanelWithBounds("PanelTest", bounds);

  // Change panel to title-only and check its height.
  panel->SetExpansionState(Panel::TITLE_ONLY);
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(panel->TitleOnlyHeight(), panel->GetBounds().height());
  EXPECT_EQ(0, GetPanelView(panel)->height()); // client area height.

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelViewTest, CheckMinimizedHeight) {
  gfx::Rect bounds(0, 0, 200, 50);
  Panel* panel = CreatePanelWithBounds("PanelTest", bounds);

  // Change panel to minimized and check its height.
  panel->SetExpansionState(Panel::MINIMIZED);
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(panel::kMinimizedPanelHeight, panel->GetBounds().height());
  EXPECT_EQ(0, GetPanelView(panel)->height()); // client area height.

  panel->Close();
}
