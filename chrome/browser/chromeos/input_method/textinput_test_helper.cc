// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method_factory.h"

namespace chromeos {
namespace {
ui::MockInputMethod* GetInputMethod() {
  ui::MockInputMethod* input_method = static_cast<ui::MockInputMethod*>(
      ash::Shell::GetPrimaryRootWindow()->GetProperty(
          aura::client::kRootWindowInputMethodKey));
  CHECK(input_method);
  return input_method;
}
}  // namespace

void TextInputTestBase::SetUpInProcessBrowserTestFixture() {
  ui::SetUpInputMethodFacotryForTesting();
}

TextInputTestHelper::TextInputTestHelper()
  : waiting_type_(NO_WAIT),
    focus_state_(false),
    latest_text_input_type_(ui::TEXT_INPUT_TYPE_NONE) {
  GetInputMethod()->AddObserver(this);
}

TextInputTestHelper::~TextInputTestHelper() {
  GetInputMethod()->RemoveObserver(this);
}

std::string TextInputTestHelper::GetSurroundingText() const {
  return surrounding_text_;
}

gfx::Rect TextInputTestHelper::GetCaretRect() const {
  return caret_rect_;
}

gfx::Rect TextInputTestHelper::GetCompositionHead() const {
  return composition_head_;
}

ui::Range TextInputTestHelper::GetSelectionRange() const {
  return selection_range_;
}

bool TextInputTestHelper::GetFocusState() const {
  return focus_state_;
}

ui::TextInputType TextInputTestHelper::GetTextInputType() const {
  return latest_text_input_type_;
}

void TextInputTestHelper::OnTextInputTypeChanged(
    const ui::TextInputClient* client) {
  latest_text_input_type_ = client->GetTextInputType();
  if (waiting_type_ == WAIT_ON_TEXT_INPUT_TYPE_CHANGED)
    MessageLoop::current()->Quit();
}

void TextInputTestHelper::OnFocus() {
  focus_state_ = true;
  if (waiting_type_ == WAIT_ON_FOCUS)
    MessageLoop::current()->Quit();
}

void TextInputTestHelper::OnBlur() {
  focus_state_ = false;
  if (waiting_type_ == WAIT_ON_BLUR)
    MessageLoop::current()->Quit();
}

void TextInputTestHelper::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (waiting_type_ == WAIT_ON_CARET_BOUNDS_CHANGED)
    MessageLoop::current()->Quit();
}

void TextInputTestHelper::WaitForTextInputStateChanged(
    ui::TextInputType expected_type) {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_TEXT_INPUT_TYPE_CHANGED;
  while (latest_text_input_type_ != expected_type)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForFocus() {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_FOCUS;
  while (focus_state_)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForBlur() {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_BLUR;
  while (!focus_state_)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForCaretBoundsChanged(
    const gfx::Rect& expected_caret_rect,
    const gfx::Rect& expected_composition_head) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_caret_rect != caret_rect_ ||
         expected_composition_head != composition_head_)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForSurroundingTextChanged(
    const std::string& expected_text,
    const ui::Range& expected_selection) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_text != surrounding_text_ ||
         expected_selection != selection_range_)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

} // namespace chromeos
