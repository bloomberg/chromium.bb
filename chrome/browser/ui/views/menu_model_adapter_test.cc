// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/view_event_test_base.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kTopMenuBaseId = 100;
const int kSubMenuBaseId = 200;

// Implement most of the ui::MenuModel pure virtual methods for subclasses
//
// Exceptions:
//  virtual int GetItemCount() const = 0;
//  virtual ItemType GetTypeAt(int index) const = 0;
//  virtual int GetCommandIdAt(int index) const = 0;
//  virtual string16 GetLabelAt(int index) const = 0;
class CommonMenuModel : public ui::MenuModel {
 public:
  CommonMenuModel() {
  }

  virtual ~CommonMenuModel() {
  }

 protected:
  // ui::MenuModel implementation.
  virtual bool HasIcons() const OVERRIDE {
    return false;
  }

  virtual bool IsItemDynamicAt(int index) const OVERRIDE {
    return false;
  }

  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const OVERRIDE {
    return false;
  }

  virtual ui::MenuSeparatorType GetSeparatorTypeAt(int index) const OVERRIDE {
    return ui::NORMAL_SEPARATOR;
  }

  virtual bool IsItemCheckedAt(int index) const OVERRIDE {
    return false;
  }

  virtual int GetGroupIdAt(int index) const OVERRIDE {
    return 0;
  }

  virtual bool GetIconAt(int index, gfx::Image* icon) OVERRIDE {
    return false;
  }

  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(
      int index) const OVERRIDE {
    return NULL;
  }

  virtual bool IsEnabledAt(int index) const OVERRIDE {
    return true;
  }

  virtual ui::MenuModel* GetSubmenuModelAt(int index) const OVERRIDE {
    return NULL;
  }

  virtual void HighlightChangedTo(int index) OVERRIDE {
  }

  virtual void ActivatedAt(int index) OVERRIDE {
  }

  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) OVERRIDE {
  }

  virtual ui::MenuModelDelegate* GetMenuModelDelegate() const OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CommonMenuModel);
};

class SubMenuModel : public CommonMenuModel {
 public:
  SubMenuModel()
      : showing_(false) {
  }

  ~SubMenuModel() {
  }

  bool showing() const {
    return showing_;
  }

 private:
  // ui::MenuModel implementation.
  virtual int GetItemCount() const OVERRIDE {
    return 1;
  }

  virtual ItemType GetTypeAt(int index) const OVERRIDE {
    return TYPE_COMMAND;
  }

  virtual int GetCommandIdAt(int index) const OVERRIDE {
    return index + kSubMenuBaseId;
  }

  virtual string16 GetLabelAt(int index) const OVERRIDE {
    return ASCIIToUTF16("Item");
  }

  virtual void MenuWillShow() {
    showing_ = true;
  }

  // Called when the menu has been closed.
  virtual void MenuClosed() {
    showing_ = false;
  }

  bool showing_;

  DISALLOW_COPY_AND_ASSIGN(SubMenuModel);
};

class TopMenuModel : public CommonMenuModel {
 public:
  TopMenuModel() {
  }

  ~TopMenuModel() {
  }

  bool IsSubmenuShowing() {
    return sub_menu_model_.showing();
  }

 private:
  // ui::MenuModel implementation.
  virtual int GetItemCount() const OVERRIDE {
    return 1;
  }

  virtual ItemType GetTypeAt(int index) const OVERRIDE {
    return TYPE_SUBMENU;
  }

  virtual int GetCommandIdAt(int index) const OVERRIDE {
    return index + kTopMenuBaseId;
  }

  virtual string16 GetLabelAt(int index) const OVERRIDE {
    return ASCIIToUTF16("submenu");
  }

  virtual MenuModel* GetSubmenuModelAt(int index) const OVERRIDE {
    return &sub_menu_model_;
  }

  mutable SubMenuModel sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(TopMenuModel);
};

}  // namespace

class MenuModelAdapterTest : public ViewEventTestBase,
                             public views::MenuButtonListener {
 public:
  MenuModelAdapterTest()
      : ViewEventTestBase(),
        button_(NULL),
        menu_model_adapter_(&top_menu_model_),
        menu_(NULL) {
    old_views_delegate_ = views::ViewsDelegate::views_delegate;
    views::ViewsDelegate::views_delegate = &views_delegate_;
  }

  virtual ~MenuModelAdapterTest() {
    views::ViewsDelegate::views_delegate = old_views_delegate_;
  }

  // ViewEventTestBase implementation.

  virtual void SetUp() OVERRIDE {
    button_ = new views::MenuButton(
        NULL, ASCIIToUTF16("Menu Adapter Test"), this, true);

    menu_ = menu_model_adapter_.CreateMenu();
    menu_runner_.reset(new views::MenuRunner(menu_));

    ViewEventTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    menu_runner_.reset(NULL);
    menu_ = NULL;
    ViewEventTestBase::TearDown();
  }

  virtual views::View* CreateContentsView() OVERRIDE {
    return button_;
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return button_->GetPreferredSize();
  }

  // views::MenuButtonListener implementation.
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE {
    gfx::Point screen_location;
    views::View::ConvertPointToScreen(source, &screen_location);
    gfx::Rect bounds(screen_location, source->size());
    ignore_result(menu_runner_->RunMenuAt(
        source->GetWidget(),
        button_,
        bounds,
        views::MenuItemView::TOPLEFT,
        views::MenuRunner::HAS_MNEMONICS));
  }

  // ViewEventTestBase implementation
  virtual void DoTestOnMessageLoop() OVERRIDE {
    Click(button_, CreateEventTask(this, &MenuModelAdapterTest::Step1));
  }

  // Open the submenu.
  void Step1() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    // Click the first item to open the submenu.
    views::MenuItemView* item = topmenu->GetMenuItemAt(0);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuModelAdapterTest::Step2));
  }

  // Rebuild the menu which should close the submenu.
  void Step2() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_TRUE(top_menu_model_.IsSubmenuShowing());

    menu_model_adapter_.BuildMenu(menu_);

    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        CreateEventTask(this, &MenuModelAdapterTest::Step3));
  }

  // Verify that the submenu MenuModel received the close callback
  // and close the menu.
  void Step3() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    // Click the button to exit the menu.
    Click(button_, CreateEventTask(this, &MenuModelAdapterTest::Step4));
  }

  // All done.
  void Step4() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_FALSE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    Done();
  }

 private:
  // Generate a mouse click on the specified view and post a new task.
  virtual void Click(views::View* view, const base::Closure& next) {
    ui_test_utils::MoveMouseToCenterAndPress(
        view,
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        next);
  }

  views::ViewsDelegate* old_views_delegate_;
  views::TestViewsDelegate views_delegate_;

  views::MenuButton* button_;
  TopMenuModel top_menu_model_;
  views::MenuModelAdapter menu_model_adapter_;
  views::MenuItemView* menu_;
  scoped_ptr<views::MenuRunner> menu_runner_;
};

#if defined(OS_WIN)
#define MAYBE_RebuildMenu DISABLED_RebuildMenu
#else
#define MAYBE_RebuildMenu RebuildMenu
#endif
VIEW_TEST(MenuModelAdapterTest, MAYBE_RebuildMenu)
