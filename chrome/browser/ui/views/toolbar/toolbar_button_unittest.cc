// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <utility>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace test {

// Friend of ToolbarButton to access private members.
class ToolbarButtonTestApi {
 public:
  explicit ToolbarButtonTestApi(ToolbarButton* button) : button_(button) {}
  ToolbarButtonTestApi(const ToolbarButtonTestApi&) = delete;
  ToolbarButtonTestApi& operator=(const ToolbarButtonTestApi&) = delete;

  views::MenuRunner* menu_runner() { return button_->menu_runner_.get(); }
  bool menu_showing() const { return button_->menu_showing_; }
  const gfx::Insets last_paint_insets() const {
    return button_->last_paint_insets_;
  }
  const gfx::Insets layout_inset_delta() const {
    return button_->layout_inset_delta_;
  }
  const base::Optional<SkColor> last_border_color() const {
    return button_->last_border_color_;
  }
  void SetAnimationTimingForTesting() {
    button_->highlight_color_animation_.highlight_color_animation_
        .SetSlideDuration(base::TimeDelta());
  }

 private:
  ToolbarButton* button_;
};

}  // namespace test

namespace {

class CheckActiveWebContentsMenuModel : public ui::SimpleMenuModel {
 public:
  explicit CheckActiveWebContentsMenuModel(TabStripModel* tab_strip_model)
      : SimpleMenuModel(nullptr), tab_strip_model_(tab_strip_model) {
    DCHECK(tab_strip_model_);
  }
  CheckActiveWebContentsMenuModel(const CheckActiveWebContentsMenuModel&) =
      delete;
  CheckActiveWebContentsMenuModel& operator=(
      const CheckActiveWebContentsMenuModel&) = delete;
  ~CheckActiveWebContentsMenuModel() override = default;

  // ui::SimpleMenuModel:
  int GetItemCount() const override {
    EXPECT_TRUE(tab_strip_model_->GetActiveWebContents());
    return 0;
  }

 private:
  TabStripModel* const tab_strip_model_;
};

class TestToolbarButton : public ToolbarButton {
 public:
  using ToolbarButton::ToolbarButton;

  void ResetBorderUpdateFlag() { did_border_update_ = false; }
  bool did_border_update() const { return did_border_update_; }

  // ToolbarButton:
  void SetBorder(std::unique_ptr<views::Border> b) override {
    ToolbarButton::SetBorder(std::move(b));
    did_border_update_ = true;
  }

 private:
  bool did_border_update_ = false;
};

}  // namespace

using ToolbarButtonViewsTest = ChromeViewsTestBase;

TEST_F(ToolbarButtonViewsTest, MenuDoesNotShowWhenTabStripIsEmpty) {
  TestTabStripModelDelegate delegate;
  TestingProfile profile;
  TabStripModel tab_strip(&delegate, &profile);
  EXPECT_FALSE(tab_strip.GetActiveWebContents());
  auto model = std::make_unique<CheckActiveWebContentsMenuModel>(&tab_strip);

  // ToolbarButton needs a parent view on some platforms.
  auto parent_view = std::make_unique<views::View>();
  ToolbarButton* button = parent_view->AddChildView(
      std::make_unique<ToolbarButton>(nullptr, std::move(model), &tab_strip));
  std::unique_ptr<views::Widget> widget_ = CreateTestWidget();
  widget_->SetContentsView(parent_view.release());

  // Since |tab_strip| is empty, calling this does not do anything. This is the
  // expected result. If it actually tries to show the menu, then
  // CheckActiveWebContentsMenuModel::GetItemCount() will get called and the
  // EXPECT_TRUE() call inside will fail.
  button->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
}

class ToolbarButtonUITest : public ChromeViewsTestBase {
 public:
  ToolbarButtonUITest() = default;
  ToolbarButtonUITest(const ToolbarButtonUITest&) = delete;
  ToolbarButtonUITest& operator=(const ToolbarButtonUITest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Usually a BackForwardMenuModel is used, but that needs a Browser*. Make
    // something simple with at least one item so a menu gets shown. Note that
    // ToolbarButton takes ownership of the |model|.
    auto model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model->AddItem(0, base::string16());
    auto button =
        std::make_unique<TestToolbarButton>(nullptr, std::move(model), nullptr);
    button_ = button.get();

    widget_ = CreateTestWidget();
    widget_->SetContentsView(button.release());
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

 protected:
  TestToolbarButton* button_ = nullptr;

 private:
  std::unique_ptr<views::Widget> widget_;
};

// Test showing and dismissing a menu to verify menu delegate lifetime.
TEST_F(ToolbarButtonUITest, ShowMenu) {
  test::ToolbarButtonTestApi test_api(button_);

  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->state());

  // Show the menu. Note that it is asynchronous.
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(test_api.menu_showing());
  EXPECT_TRUE(test_api.menu_runner());
  EXPECT_TRUE(test_api.menu_runner()->IsRunning());

  // Button should appear pressed when the menu is showing.
  EXPECT_EQ(views::Button::STATE_PRESSED, button_->state());

  test_api.menu_runner()->Cancel();

  // Ensure the ToolbarButton's |menu_runner_| member is reset to null.
  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->state());
}

// Test deleting a ToolbarButton while its menu is showing.
TEST_F(ToolbarButtonUITest, DeleteWithMenu) {
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_TRUE(test::ToolbarButtonTestApi(button_).menu_runner());
  widget()->SetContentsView(new views::View());  // Deletes |button_|.
}

// Tests to make sure the button's border is updated as its height changes.
TEST_F(ToolbarButtonUITest, TestBorderUpdateHeightChange) {
  const gfx::Insets toolbar_padding = GetLayoutInsets(TOOLBAR_BUTTON);

  button_->ResetBorderUpdateFlag();
  for (int bounds_height : {8, 12, 20}) {
    EXPECT_FALSE(button_->did_border_update());
    button_->SetBoundsRect({bounds_height, bounds_height});
    EXPECT_TRUE(button_->did_border_update());
    EXPECT_EQ(button_->border()->GetInsets(), gfx::Insets(toolbar_padding));
    button_->ResetBorderUpdateFlag();
  }
}

// Tests to make sure the button's border color is updated as its animation
// color changes.
TEST_F(ToolbarButtonUITest, TestBorderUpdateColorChange) {
  test::ToolbarButtonTestApi test_api(button_);
  test_api.SetAnimationTimingForTesting();

  button_->ResetBorderUpdateFlag();
  for (SkColor border_color : {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE}) {
    EXPECT_FALSE(button_->did_border_update());
    button_->SetHighlight(base::string16(), border_color);
    EXPECT_EQ(button_->border()->color(), border_color);
    EXPECT_TRUE(button_->did_border_update());
    button_->ResetBorderUpdateFlag();
  }
}
