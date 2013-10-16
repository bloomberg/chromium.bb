// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/view_event_test_base.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/button/button_dropdown.h"

class ButtonDropDownDragTest : public ViewEventTestBase,
                               ui::SimpleMenuModel::Delegate {
 public:
  ButtonDropDownDragTest()
      : button_(NULL),
        menu_shown_(false),
        menu_closed_(false) {
  }

  virtual ~ButtonDropDownDragTest() {
  }

  // ViewEventTestBase implementation.
  virtual void SetUp() OVERRIDE {
    button_ = new views::ButtonDropDown(NULL, new ui::SimpleMenuModel(this));

    ViewEventTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ViewEventTestBase::TearDown();
  }

  virtual views::View* CreateContentsView() OVERRIDE {
    return button_;
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return button_->GetPreferredSize();
  }

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int id) const OVERRIDE {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int id, int event_flags) OVERRIDE {
  }

  virtual void MenuWillShow(ui::SimpleMenuModel* /*source*/) OVERRIDE {
    menu_shown_ = true;
  }

  virtual void MenuClosed(ui::SimpleMenuModel* /*source*/) OVERRIDE {
    menu_closed_ = true;
  }

  // ViewEventTestBase implementation.
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Click on the ButtonDropDown.
    ui_test_utils::MoveMouseToCenterAndPress(
        button_,
        ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &ButtonDropDownDragTest::Step1));
  }

  void Step1() {
    // Drag to invoke the menu.
    gfx::Point view_center;
    views::View::ConvertPointToScreen(button_, &view_center);
    // The 50 is a bit arbitrary. We just need a value greater than the drag
    // threshold.
    ui_controls::SendMouseMoveNotifyWhenDone(
        view_center.x(), view_center.y() + 50,
        CreateEventTask(this, &ButtonDropDownDragTest::Step2));
  }

  void Step2() {
    ASSERT_TRUE(menu_shown_);

    // Release.
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT,
        ui_controls::UP,
        CreateEventTask(this, &ButtonDropDownDragTest::Step3));
  }

  void Step3() {
    // Click mouse to dismiss menu.  The views menu does not dismiss the
    // menu on click-drag-release unless an item is selected.
    ui_test_utils::MoveMouseToCenterAndPress(
        button_,
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &ButtonDropDownDragTest::Step4));
  }

  void Step4() {
    // One more hop is required because ui::SimpleMenuModel calls
    // ui::SimpleMenuModel::Delegate::MenuClosed() via a posted
    // task.
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &ButtonDropDownDragTest::Step5));
  }

  void Step5() {
    ASSERT_TRUE(menu_closed_);
    Done();
  }

 private:
  views::ButtonDropDown* button_;
  bool menu_shown_;
  bool menu_closed_;
};

VIEW_TEST(ButtonDropDownDragTest, DragActivation)
