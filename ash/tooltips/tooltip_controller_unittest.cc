// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font.h"
#include "ui/gfx/point.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_client.h"

using views::corewm::TooltipController;
using views::corewm::test::TooltipTestView;
using views::corewm::test::TooltipControllerTestHelper;

// The tests in this file exercise bits of TooltipController that are hard to
// test outside of ash. Meaning these tests require the shell and related things
// to be installed.

namespace ash {
namespace test {

namespace {

views::Widget* CreateNewWidgetWithBoundsOn(int display,
                                           const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = Shell::GetAllRootWindows().at(display);
  params.bounds = bounds;
  widget->Init(params);
  widget->Show();
  return widget;
}

views::Widget* CreateNewWidgetOn(int display) {
  return CreateNewWidgetWithBoundsOn(display, gfx::Rect());
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
  contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(gfx::Rect(widget->GetWindowBoundsInScreen().origin(),
                              contents_view_bounds.size()));
}

TooltipController* GetController() {
  return static_cast<TooltipController*>(
      aura::client::GetTooltipClient(Shell::GetPrimaryRootWindow()));
}

}  // namespace

class TooltipControllerTest : public AshTestBase {
 public:
  TooltipControllerTest() {}
  virtual ~TooltipControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    helper_.reset(new TooltipControllerTestHelper(GetController()));
  }

 protected:
  scoped_ptr<TooltipControllerTestHelper> helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, NonNullTooltipClient) {
  EXPECT_TRUE(aura::client::GetTooltipClient(Shell::GetPrimaryRootWindow())
              != NULL);
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, HideTooltipWhenCursorHidden) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                view->bounds().CenterPoint());
  base::string16 expected_tooltip = base::ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Hide the cursor and check again.
  ash::Shell::GetInstance()->cursor_manager()->DisableMouseEvents();
  helper_->FireTooltipTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Show the cursor and re-check.
  RunAllPendingInMessageLoop();
  ash::Shell::GetInstance()->cursor_manager()->EnableMouseEvents();
  RunAllPendingInMessageLoop();
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TooltipsOnMultiDisplayShouldNotCrash) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  scoped_ptr<views::Widget> widget1(CreateNewWidgetWithBoundsOn(
      0, gfx::Rect(10, 10, 100, 100)));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget1.get(), view1);
  view1->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(widget1->GetNativeView()->GetRootWindow(), root_windows[0]);

  scoped_ptr<views::Widget> widget2(CreateNewWidgetWithBoundsOn(
      1, gfx::Rect(1200, 10, 100, 100)));
  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget2.get(), view2);
  view2->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text for view 2"));
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[1]);

  // Show tooltip on second display.
  ui::test::EventGenerator generator(root_windows[1]);
  generator.MoveMouseRelativeTo(widget2->GetNativeView(),
                                view2->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Get rid of secondary display. This destroy's the tooltip's aura window. If
  // we have handled this case, we will not crash in the following statement.
  UpdateDisplay("1000x600");
#if !defined(OS_WIN)
  // TODO(cpu): Detangle the window destruction notification. Currently
  // the TooltipController::OnWindowDestroyed is not being called then the
  // display is torn down so the tooltip is is still there.
  EXPECT_FALSE(helper_->IsTooltipVisible());
#endif
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[0]);

  // The tooltip should create a new aura window for itself, so we should still
  // be able to show tooltips on the primary display.
  ui::test::EventGenerator generator1(root_windows[0]);
  generator1.MoveMouseRelativeTo(widget1->GetNativeView(),
                                 view1->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

}  // namespace test
}  // namespace ash
