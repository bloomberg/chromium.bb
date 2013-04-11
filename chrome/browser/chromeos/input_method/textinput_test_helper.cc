// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
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
    selection_range_(ui::Range::InvalidRange()),
    focus_state_(false),
    latest_text_input_type_(ui::TEXT_INPUT_TYPE_NONE) {
  GetInputMethod()->AddObserver(this);
}

TextInputTestHelper::~TextInputTestHelper() {
  GetInputMethod()->RemoveObserver(this);
}

string16 TextInputTestHelper::GetSurroundingText() const {
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

ui::TextInputClient* TextInputTestHelper::GetTextInputClient() const {
  return GetInputMethod()->GetTextInputClient();
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
  ui::Range text_range;
  if (!GetTextInputClient()->GetTextRange(&text_range) ||
      !GetTextInputClient()->GetTextFromRange(text_range, &surrounding_text_) ||
      !GetTextInputClient()->GetSelectionRange(&selection_range_))
      return;
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
    const string16& expected_text,
    const ui::Range& expected_selection) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_text != surrounding_text_ ||
         expected_selection != selection_range_)
    content::RunMessageLoop();
  waiting_type_ = NO_WAIT;
}

// static
bool TextInputTestHelper::ConvertRectFromString(const std::string& str,
                                                gfx::Rect* rect) {
  DCHECK(rect);
  std::vector<std::string> rect_piece;
  if (Tokenize(str, ",", &rect_piece) != 4UL)
    return false;
  int x, y, width, height;
  if (!base::StringToInt(rect_piece[0], &x))
    return false;
  if (!base::StringToInt(rect_piece[1], &y))
    return false;
  if (!base::StringToInt(rect_piece[2], &width))
    return false;
  if (!base::StringToInt(rect_piece[3], &height))
    return false;
  *rect = gfx::Rect(x, y, width, height);
  return true;
}

// static
bool TextInputTestHelper::ClickElement(const std::string& id,
                                       content::WebContents* tab) {
  std::string coordinate;
  if (!content::ExecuteScriptAndExtractString(
      tab,
      "textinput_helper.retrieveElementCoordinate('" + id + "')",
      &coordinate))
    return false;
  gfx::Rect rect;
  if (!ConvertRectFromString(coordinate, &rect))
    return false;

  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.x = rect.CenterPoint().x();
  mouse_event.y = rect.CenterPoint().y();
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  return true;
}

} // namespace chromeos
