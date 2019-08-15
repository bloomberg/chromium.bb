// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/color_picker_view.h"

#include <array>
#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_codes.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ColorPickerViewTest : public ChromeViewsTestBase {
 protected:
  static const std::array<std::pair<SkColor, base::string16>, 3> kTestColors;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    views::Widget::InitParams widget_params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(widget_params));

    color_picker_ = new ColorPickerView(kTestColors);
    widget_->SetContentsView(color_picker_);

    color_picker_->SizeToPreferredSize();
  }

  void TearDown() override {
    widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  void ClickColorElement(views::Button* element) {
    gfx::Point center = element->GetLocalBounds().CenterPoint();
    gfx::Point root_center = center;
    views::View::ConvertPointToWidget(color_picker_, &root_center);

    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, center, root_center,
                                 base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                                 0);
    element->OnMousePressed(pressed_event);

    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, center, root_center,
                                  base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                                  0);
    element->OnMouseReleased(released_event);
  }

  void ClickColorAtIndex(int index) {
    ClickColorElement(color_picker_->GetElementAtIndexForTesting(index));
  }

  ColorPickerView* color_picker_;

 private:
  std::unique_ptr<views::Widget> widget_;
};

// static
const std::array<std::pair<SkColor, base::string16>, 3>
    ColorPickerViewTest::kTestColors{{
        {SK_ColorRED, base::ASCIIToUTF16("Red")},
        {SK_ColorGREEN, base::ASCIIToUTF16("Green")},
        {SK_ColorBLUE, base::ASCIIToUTF16("Blue")},
    }};

TEST_F(ColorPickerViewTest, NoColorSelectedByDefault) {
  EXPECT_FALSE(color_picker_->GetSelectedColor().has_value());
}

TEST_F(ColorPickerViewTest, ClickingSelectsColor) {
  ClickColorAtIndex(0);
  EXPECT_EQ(kTestColors[0].first, color_picker_->GetSelectedColor());

  ClickColorAtIndex(1);
  EXPECT_EQ(kTestColors[1].first, color_picker_->GetSelectedColor());
}

TEST_F(ColorPickerViewTest, ClickingTwiceDeselects) {
  ClickColorAtIndex(0);
  ClickColorAtIndex(0);
  EXPECT_FALSE(color_picker_->GetSelectedColor().has_value());
}

TEST_F(ColorPickerViewTest, KeyboardFocusBehavesLikeRadioButtons) {
  views::FocusManager* focus_manager = color_picker_->GetFocusManager();

  // When no color is selected, focus should start on the first.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(0),
            focus_manager->GetFocusedView());

  // Pressing arrow keys should cycle through the elements.
  ui::KeyEvent arrow_event(
      ui::EventType::ET_KEY_PRESSED,
      ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::ARROW_RIGHT),
      ui::DomCode::ARROW_RIGHT, ui::EF_NONE);
  EXPECT_FALSE(focus_manager->OnKeyEvent(arrow_event));
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(1),
            focus_manager->GetFocusedView());

  focus_manager->ClearFocus();
  ClickColorAtIndex(1);

  // Re-entering should restore focus to the currently selected color.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(1),
            focus_manager->GetFocusedView());
}
