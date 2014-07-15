// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/menu_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"

MenuTestBase::MenuTestBase()
    : ViewEventTestBase(),
      button_(NULL),
      menu_(NULL),
      last_command_(0) {
}

MenuTestBase::~MenuTestBase() {
}

void MenuTestBase::Click(views::View* view, const base::Closure& next) {
  ui_test_utils::MoveMouseToCenterAndPress(
      view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      next);
}

void MenuTestBase::KeyPress(ui::KeyboardCode keycode,
                            const base::Closure& next) {
  ui_controls::SendKeyPressNotifyWhenDone(
      GetWidget()->GetNativeView()->GetRootWindow(), keycode, false, false,
      false, false, next);
}

void MenuTestBase::SetUp() {
  button_ = new views::MenuButton(
      NULL, base::ASCIIToUTF16("Menu Test"), this, true);
  menu_ = new views::MenuItemView(this);
  BuildMenu(menu_);
  menu_runner_.reset(
      new views::MenuRunner(menu_, views::MenuRunner::HAS_MNEMONICS));

  ViewEventTestBase::SetUp();
}

void MenuTestBase::TearDown() {
  menu_runner_.reset();
  menu_ = NULL;
  ViewEventTestBase::TearDown();
}

views::View* MenuTestBase::CreateContentsView() {
  return button_;
}

void MenuTestBase::DoTestOnMessageLoop() {
  Click(button_, CreateEventTask(this, &MenuTestBase::DoTestWithMenuOpen));
}

gfx::Size MenuTestBase::GetPreferredSize() const {
  return button_->GetPreferredSize();
}

void MenuTestBase::OnMenuButtonClicked(views::View* source,
                                       const gfx::Point& point) {
  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  ignore_result(menu_runner_->RunMenuAt(source->GetWidget(),
                                        button_,
                                        bounds,
                                        views::MENU_ANCHOR_TOPLEFT,
                                        ui::MENU_SOURCE_NONE));
}

void MenuTestBase::ExecuteCommand(int id) {
  last_command_ = id;
}
