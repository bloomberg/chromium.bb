// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tooltips/tooltip_controller.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/tooltip_client.h"
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

views::Widget* CreateNewWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetRootWindow();
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
      aura::client::GetTooltipClient(Shell::GetRootWindow()));
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
  EXPECT_TRUE(aura::client::GetTooltipClient(Shell::GetRootWindow()) != NULL);
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());
  EXPECT_FALSE(IsTooltipVisible());
}

TEST_F(TooltipControllerTest, ViewTooltip) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());
  aura::test::EventGenerator generator(Shell::GetRootWindow());
  generator.MoveMouseToCenterOf(widget->GetNativeView());

  aura::Window* window = widget->GetNativeView();
  EXPECT_EQ(window, Shell::GetRootWindow()->GetEventHandlerForPoint(
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
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view1);
  view1->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view2);

  aura::Window* window = widget->GetNativeView();

  // Fire tooltip timer so tooltip becomes visible.
  aura::test::EventGenerator generator(Shell::GetRootWindow());
  generator.MoveMouseRelativeTo(window,
                                view1->bounds().CenterPoint());
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());
  for (int i = 0; i < 50; i++) {
    generator.MoveMouseBy(1, 0);
    EXPECT_TRUE(IsTooltipVisible());
    EXPECT_EQ(window,
        Shell::GetRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
  for (int i = 0; i < 50; i++) {
    generator.MoveMouseBy(1, 0);
    EXPECT_FALSE(IsTooltipVisible());
    EXPECT_EQ(window,
        Shell::GetRootWindow()->GetEventHandlerForPoint(
            generator.current_location()));
    string16 expected_tooltip;  // = ""
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, GetTooltipText());
    EXPECT_EQ(window, GetTooltipWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  aura::test::EventGenerator generator(Shell::GetRootWindow());
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
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(string16(), GetTooltipText());
  EXPECT_EQ(NULL, GetTooltipWindow());

  aura::test::EventGenerator generator(Shell::GetRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                view->bounds().CenterPoint());
  string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  FireTooltipTimer();
  EXPECT_TRUE(IsTooltipVisible());

  // Hide the cursor and check again.
  Shell::GetRootWindow()->ShowCursor(false);
  FireTooltipTimer();
  EXPECT_FALSE(IsTooltipVisible());

  // Show the cursor and re-check.
  Shell::GetRootWindow()->ShowCursor(true);
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

}  // namespace test
}  // namespace ash
