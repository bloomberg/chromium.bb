// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/wm/cursor_manager.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"
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

views::Widget* CreateNewWidgetWithBoundsOn(int display,
                                           const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = Shell::GetAllRootWindows().at(display);
  params.child = true;
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

ash::internal::TooltipController* GetController() {
  return static_cast<ash::internal::TooltipController*>(
      aura::client::GetTooltipClient(Shell::GetPrimaryRootWindow()));
}

gfx::Font GetDefaultFont() {
  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
}

}  // namespace

class TooltipControllerTest : public AshTestBase {
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

  bool IsTooltipTimerRunning() {
    return GetController()->tooltip_timer_.IsRunning();
  }

  void FireTooltipShownTimer() {
    GetController()->tooltip_shown_timer_.Stop();
    GetController()->TooltipShownTimerFired();
  }

  bool IsTooltipShownTimerRunning() {
    return GetController()->tooltip_shown_timer_.IsRunning();
  }

  bool IsTooltipVisible() {
    return GetController()->IsTooltipVisible();
  }

  void TrimTooltipToFit(string16* text,
                        int* max_width,
                        int* line_count,
                        int x,
                        int y) {
    ash::internal::TooltipController::TrimTooltipToFit(text, max_width,
        line_count, x, y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, NonNullTooltipClient) {
  EXPECT_TRUE(aura::client::GetTooltipClient(Shell::GetPrimaryRootWindow())
              != NULL);
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());
  EXPECT_FALSE(IsTooltipVisible());
}

TEST_F(TooltipControllerTest, ViewTooltip) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseToCenterOf(widget->GetNativeView());

  aura::Window* window = widget->GetNativeView();
  EXPECT_EQ(window, Shell::GetPrimaryRootWindow()->GetEventHandlerForPoint(
      generator.current_location()));
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  FireTooltipTimer();

  EXPECT_TRUE(IsTooltipVisible());
  generator.MoveMouseBy(1, 0);

  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipsInMultipleViews) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view2);

  aura::Window* window = widget->GetNativeView();

  // Fire tooltip timer so tooltip becomes visible.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(window,
                                view1->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  for (int i = 0; i < 49; ++i) {
    generator.MoveMouseBy(1, 0);
    EXPECT_TRUE(IsTooltipVisible());
    EXPECT_EQ(window,
        Shell::GetPrimaryRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
  for (int i = 0; i < 49; ++i) {
    generator.MoveMouseBy(1, 0);
    EXPECT_FALSE(IsTooltipVisible());
    EXPECT_EQ(window,
        Shell::GetPrimaryRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip;  // = ""
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                view->bounds().CenterPoint());
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

TEST_F(TooltipControllerTest, HideTooltipWhenCursorHidden) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                view->bounds().CenterPoint());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());

  // Hide the cursor and check again.
  ash::Shell::GetInstance()->cursor_manager()->DisableMouseEvents();
  FireTooltipTimer();
  EXPECT_FALSE(IsTooltipVisible());

  // Show the cursor and re-check.
  ash::Shell::GetInstance()->cursor_manager()->EnableMouseEvents();
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TrimTooltipToFitTests) {
  string16 tooltip;
  int max_width, line_count, expect_lines;
  int max_pixel_width = 400;  // copied from constants in tooltip_controller.cc
  int max_lines = 10;  // copied from constants in tooltip_controller.cc
  gfx::Font font = GetDefaultFont();
  size_t tooltip_len;

  // Error in computed size vs. expected size should not be greater than the
  // size of the longest word.
  int error_in_pixel_width = font.GetStringWidth(ASCIIToUTF16("tooltip"));

  // Long tooltips should wrap to next line
  tooltip.clear();
  max_width = line_count = -1;
  expect_lines = 3;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 1) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  tooltip_len = tooltip.length();
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_NEAR(max_pixel_width, max_width, error_in_pixel_width);
  EXPECT_EQ(expect_lines, line_count);
  EXPECT_EQ(tooltip_len + expect_lines - 1, tooltip.length());

  // More than |max_lines| lines should get truncated at 10 lines.
  tooltip.clear();
  max_width = line_count = -1;
  expect_lines = 13;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 1) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_NEAR(max_pixel_width, max_width, error_in_pixel_width);
  EXPECT_EQ(max_lines, line_count);

  // Long multi line tooltips should wrap individual lines.
  tooltip.clear();
  max_width = line_count = -1;
  expect_lines = 4;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 2) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  tooltip.insert(tooltip.length() / 2, ASCIIToUTF16("\n"));
  tooltip_len = tooltip.length();
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_NEAR(max_pixel_width, max_width, error_in_pixel_width);
  EXPECT_EQ(expect_lines, line_count);
  // We may have inserted the line break above near a space which will get
  // trimmed. Hence we may be off by 1 in the final tooltip length calculation.
  EXPECT_NEAR(tooltip_len + expect_lines - 2, tooltip.length(), 1);

#if !defined(OS_WIN)
  // Tooltip with really long word gets elided.
  tooltip.clear();
  max_width = line_count = -1;
  tooltip = UTF8ToUTF16(std::string('a', max_pixel_width));
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_NEAR(max_pixel_width, max_width, 5);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(ui::ElideText(UTF8ToUTF16(std::string('a', max_pixel_width)), font,
                          max_pixel_width, ui::ELIDE_AT_END), tooltip);
#endif

  // Normal small tooltip should stay as is.
  tooltip.clear();
  max_width = line_count = -1;
  tooltip = ASCIIToUTF16("Small Tooltip");
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_EQ(font.GetStringWidth(ASCIIToUTF16("Small Tooltip")), max_width);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(ASCIIToUTF16("Small Tooltip"), tooltip);

  // Normal small multi-line tooltip should stay as is.
  tooltip.clear();
  max_width = line_count = -1;
  tooltip = ASCIIToUTF16("Multi line\nTooltip");
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  int expected_width = font.GetStringWidth(ASCIIToUTF16("Multi line"));
  expected_width = std::max(expected_width,
                            font.GetStringWidth(ASCIIToUTF16("Tooltip")));
  EXPECT_EQ(expected_width, max_width);
  EXPECT_EQ(2, line_count);
  EXPECT_EQ(ASCIIToUTF16("Multi line\nTooltip"), tooltip);

  // Whitespaces in tooltips are preserved.
  tooltip.clear();
  max_width = line_count = -1;
  tooltip = ASCIIToUTF16("Small  Tool  t\tip");
  TrimTooltipToFit(&tooltip, &max_width, &line_count, 0, 0);
  EXPECT_EQ(font.GetStringWidth(ASCIIToUTF16("Small  Tool  t\tip")), max_width);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(ASCIIToUTF16("Small  Tool  t\tip"), tooltip);
}

TEST_F(TooltipControllerTest, TooltipHidesOnKeyPressAndStaysHiddenUntilChange) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view2);
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = widget->GetNativeView();

  // Fire tooltip timer so tooltip becomes visible.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(window,
                                view1->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_TRUE(IsTooltipShownTimerRunning());

  generator.PressKey(ui::VKEY_1, 0);
  EXPECT_FALSE(IsTooltipVisible());
  EXPECT_FALSE(IsTooltipTimerRunning());
  EXPECT_FALSE(IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; i++) {
    generator.MoveMouseBy(1, 0);
    EXPECT_FALSE(IsTooltipVisible());
    EXPECT_FALSE(IsTooltipTimerRunning());
    EXPECT_FALSE(IsTooltipShownTimerRunning());
    EXPECT_EQ(window,
        Shell::GetPrimaryRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator.MoveMouseBy(1, 0);
  EXPECT_TRUE(IsTooltipTimerRunning());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_TRUE(IsTooltipShownTimerRunning());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipHidesOnTimeoutAndStaysHiddenUntilChange) {
  scoped_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view2);
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = widget->GetNativeView();

  // Fire tooltip timer so tooltip becomes visible.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(window,
                                view1->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_TRUE(IsTooltipShownTimerRunning());

  FireTooltipShownTimer();
  EXPECT_FALSE(IsTooltipVisible());
  EXPECT_FALSE(IsTooltipTimerRunning());
  EXPECT_FALSE(IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; ++i) {
    generator.MoveMouseBy(1, 0);
    EXPECT_FALSE(IsTooltipVisible());
    EXPECT_FALSE(IsTooltipTimerRunning());
    EXPECT_FALSE(IsTooltipShownTimerRunning());
    EXPECT_EQ(window,
        Shell::GetPrimaryRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator.MoveMouseBy(1, 0);
  EXPECT_TRUE(IsTooltipTimerRunning());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  EXPECT_TRUE(IsTooltipShownTimerRunning());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, GetTooltipText());
  EXPECT_EQ(window, GetTooltipWindow());
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_TooltipsOnMultiDisplayShouldNotCrash \
        DISABLED_TooltipsOnMultiDisplayShouldNotCrash
#else
#define MAYBE_TooltipsOnMultiDisplayShouldNotCrash \
        TooltipsOnMultiDisplayShouldNotCrash
#endif

TEST_F(TooltipControllerTest, MAYBE_TooltipsOnMultiDisplayShouldNotCrash) {
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  scoped_ptr<views::Widget> widget1(CreateNewWidgetWithBoundsOn(
      0, gfx::Rect(10, 10, 100, 100)));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget1.get(), view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(widget1->GetNativeView()->GetRootWindow(), root_windows[0]);

  scoped_ptr<views::Widget> widget2(CreateNewWidgetWithBoundsOn(
      1, gfx::Rect(1200, 10, 100, 100)));
  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget2.get(), view2);
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[1]);

  // Show tooltip on second display.
  aura::test::EventGenerator generator(root_windows[1]);
  generator.MoveMouseRelativeTo(widget2->GetNativeView(),
                                view2->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());

  // Get rid of secondary display. This destroy's the tooltip's aura window. If
  // we have handled this case, we will not crash in the following statement.
  UpdateDisplay("1000x600");
#if !defined(OS_WIN)
  // TODO(cpu): Detangle the window destruction notification. Currently
  // the TooltipController::OnWindowDestroyed is not being called then the
  // display is torn down so the tooltip is is still there.
  EXPECT_FALSE(IsTooltipVisible());
#endif
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[0]);

  // The tooltip should create a new aura window for itself, so we should still
  // be able to show tooltips on the primary display.
  aura::test::EventGenerator generator1(root_windows[0]);
  generator1.MoveMouseRelativeTo(widget1->GetNativeView(),
                                 view1->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
}

}  // namespace test
}  // namespace ash
