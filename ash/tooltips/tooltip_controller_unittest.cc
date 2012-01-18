// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/aura_shell_test_base.h"
#include "ash/tooltips/tooltip_controller.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/point.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

class TooltipTestView : public views::View {
 public:
  TooltipTestView() : views::View() {
  }

  void set_tooltip_text(string16 tooltip_text) { tooltip_text_ = tooltip_text; }

  // Overridden from views::View
  bool GetTooltipText(const gfx::Point& p, string16* tooltip) const {
    *tooltip = tooltip_text_;
    return true;
  }

 private:
  string16 tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(TooltipTestView);
};

views::Widget* CreateNewWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = aura::RootWindow::GetInstance();
  params.child = true;
  widget->Init(params);
  widget->Show();
  return widget;
}

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView()) {
    views::View* contents_view = new views::View;
    widget->SetContentsView(contents_view);
  }

  views::View* contents_view = widget->GetContentsView();
  contents_view->AddChildView(view);
  view->SetBounds(contents_view->width(), 0, 100, 100);
  gfx::Rect contents_view_bounds = contents_view->bounds();
  contents_view_bounds = contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(contents_view_bounds);
}

ash::internal::TooltipController* GetController() {
  return static_cast<ash::internal::TooltipController*>(
      aura::client::GetTooltipClient());
}

void SimulateMouseMoveAtPoint(const gfx::Point& point) {
  aura::MouseEvent event(ui::ET_MOUSE_MOVED, point, 0);
  aura::RootWindow::GetInstance()->DispatchMouseEvent(&event);
}

}  // namespace

class TooltipControllerTest : public AuraShellTestBase {
 public:
  TooltipControllerTest() {}
  virtual ~TooltipControllerTest() {}

  string16 GetTooltipText() {
    return GetController()->tooltip_text_;
  }

  aura::Window* GetTooltipWindow() {
    return GetController()->tooltip_window_;
  }

  void FireTooltipTimer() {
    GetController()->TooltipTimerFired();
  }

  bool IsTooltipVisible() {
    return GetController()->IsTooltipVisible();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, NonNullTooltipClient) {
  EXPECT_TRUE(aura::client::GetTooltipClient() != NULL);
  EXPECT_EQ(ASCIIToUTF16(""), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());
  EXPECT_FALSE(IsTooltipVisible());
}

TEST_F(TooltipControllerTest, ViewTooltip) {
  views::Widget* widget = CreateNewWidget();
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget, view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(ASCIIToUTF16(""), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  gfx::Point point = gfx::Rect(view->bounds()).CenterPoint();
  SimulateMouseMoveAtPoint(point);
  aura::Window* window = widget->GetNativeView();
  EXPECT_EQ(window,
      aura::RootWindow::GetInstance()->GetEventHandlerForPoint(point));
  EXPECT_TRUE(aura::client::GetTooltipText(window) != NULL);
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, *aura::client::GetTooltipText(window));
  EXPECT_EQ(ASCIIToUTF16(""), GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  FireTooltipTimer();

  EXPECT_TRUE(IsTooltipVisible());
  point.Offset(1, 0);
  SimulateMouseMoveAtPoint(point);

  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_TRUE(aura::client::GetTooltipText(window) != NULL);
  EXPECT_EQ(expected_tooltip, *aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipsInMultipleViews) {
  views::Widget* widget = CreateNewWidget();
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget, view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(ASCIIToUTF16(""), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget, view2);

  aura::Window* window = widget->GetNativeView();
  gfx::Point point = gfx::Rect(view1->bounds()).CenterPoint();

  // Fire tooltip timer so tooltip becomes visible.
  SimulateMouseMoveAtPoint(point);
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  for (int i = 0; i < 50; i++) {
    point.Offset(1, 0);
    SimulateMouseMoveAtPoint(point);
    EXPECT_TRUE(IsTooltipVisible());
    EXPECT_EQ(window,
        aura::RootWindow::GetInstance()->GetEventHandlerForPoint(point));
    EXPECT_TRUE(aura::client::GetTooltipText(window) != NULL);
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
    EXPECT_EQ(expected_tooltip, *aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
  for (int i = 0; i < 50; i++) {
    point.Offset(1, 0);
    SimulateMouseMoveAtPoint(point);
    EXPECT_FALSE(IsTooltipVisible());
    EXPECT_EQ(window,
        aura::RootWindow::GetInstance()->GetEventHandlerForPoint(point));
    EXPECT_TRUE(aura::client::GetTooltipText(window) != NULL);
    string16 expected_tooltip = ASCIIToUTF16("");
    EXPECT_EQ(expected_tooltip, *aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  views::Widget* widget = CreateNewWidget();
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget, view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(ASCIIToUTF16(""), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  gfx::Point point = gfx::Rect(view->bounds()).CenterPoint();
  SimulateMouseMoveAtPoint(point);
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());

  // Diable tooltips and check again.
  GetController()->SetTooltipsEnabled(false);
  EXPECT_FALSE(IsTooltipVisible());
  FireTooltipTimer();
  EXPECT_FALSE(IsTooltipVisible());

  // Enable tooltips back and check again.
  GetController()->SetTooltipsEnabled(true);
  EXPECT_FALSE(IsTooltipVisible());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
}

}  // namespace test
}  // namespace ash
