// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnifier_test_utils.h"

#include "ash/shell.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

aura::Window* GetViewRootWindow(views::View* view) {
  DCHECK(view);
  return view->GetWidget()->GetNativeWindow()->GetRootWindow();
}

}  // namespace

// A view that contains a single text field for testing text input events.
class TestTextInputView : public views::WidgetDelegateView {
 public:
  TestTextInputView() : text_field_(new views::Textfield) {
    text_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
    AddChildView(text_field_);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~TestTextInputView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(50, 50);
  }

  void FocusOnTextInput() { text_field_->RequestFocus(); }

 private:
  views::Textfield* text_field_;  // owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(TestTextInputView);
};

void MagnifierTextInputTestHelper::CreateAndShowTextInputView(
    const gfx::Rect& bounds) {
  CreateAndShowTextInputViewInRoot(bounds, Shell::GetPrimaryRootWindow());
}

void MagnifierTextInputTestHelper::CreateAndShowTextInputViewInRoot(
    const gfx::Rect& bounds,
    aura::Window* root) {
  text_input_view_ = new TestTextInputView;
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      text_input_view_, root, bounds);
  widget->Show();
}

gfx::Rect MagnifierTextInputTestHelper::GetTextInputViewBounds() {
  DCHECK(text_input_view_);
  gfx::Rect bounds = text_input_view_->bounds();
  gfx::Point origin = bounds.origin();
  // Convert origin to screen coordinates.
  views::View::ConvertPointToScreen(text_input_view_, &origin);
  // Convert origin to root window coordinates.
  ::wm::ConvertPointFromScreen(GetViewRootWindow(text_input_view_), &origin);
  return gfx::Rect(origin.x(), origin.y(), bounds.width(), bounds.height());
}

gfx::Rect MagnifierTextInputTestHelper::GetCaretBounds() {
  gfx::Rect caret_bounds =
      GetInputMethod()->GetTextInputClient()->GetCaretBounds();
  gfx::Point origin = caret_bounds.origin();
  ::wm::ConvertPointFromScreen(GetViewRootWindow(text_input_view_), &origin);
  return gfx::Rect(origin.x(), origin.y(), caret_bounds.width(),
                   caret_bounds.height());
}

void MagnifierTextInputTestHelper::FocusOnTextInputView() {
  DCHECK(text_input_view_);
  text_input_view_->FocusOnTextInput();
}

ui::InputMethod* MagnifierTextInputTestHelper::GetInputMethod() {
  DCHECK(text_input_view_);
  return text_input_view_->GetWidget()->GetInputMethod();
}

}  // namespace ash
