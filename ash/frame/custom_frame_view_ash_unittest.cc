// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/custom_frame_view_ash.h"

#include <memory>

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A views::WidgetDelegate which uses a CustomFrameViewAsh.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {}
  ~TestWidgetDelegate() override {}

  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    custom_frame_view_ = new CustomFrameViewAsh(widget);
    return custom_frame_view_;
  }

  CustomFrameViewAsh* custom_frame_view() const { return custom_frame_view_; }

 private:
  // Not owned.
  CustomFrameViewAsh* custom_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

class TestWidgetConstraintsDelegate : public TestWidgetDelegate {
 public:
  TestWidgetConstraintsDelegate() {}
  ~TestWidgetConstraintsDelegate() override {}

  // views::View:
  gfx::Size GetMinimumSize() const override { return minimum_size_; }

  gfx::Size GetMaximumSize() const override { return maximum_size_; }

  views::View* GetContentsView() override {
    // Set this instance as the contents view so that the maximum and minimum
    // size constraints will be used.
    return this;
  }

  // views::WidgetDelegate:
  bool CanMaximize() const override { return true; }

  bool CanMinimize() const override { return true; }

  void set_minimum_size(const gfx::Size& min_size) { minimum_size_ = min_size; }

  void set_maximum_size(const gfx::Size& max_size) { maximum_size_ = max_size; }

  const gfx::Rect& GetFrameCaptionButtonContainerViewBounds() {
    return custom_frame_view()
        ->GetFrameCaptionButtonContainerViewForTest()
        ->bounds();
  }

  void EndFrameCaptionButtonContainerViewAnimations() {
    FrameCaptionButtonContainerView::TestApi test(
        custom_frame_view()->GetFrameCaptionButtonContainerViewForTest());
    test.EndAnimations();
  }

  int GetTitleBarHeight() const {
    return custom_frame_view()->NonClientTopBorderHeight();
  }

 private:
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetConstraintsDelegate);
};

class CustomFrameViewAshTest : public AshTestBase {
 public:
  CustomFrameViewAshTest() {}
  ~CustomFrameViewAshTest() override {}

 protected:
  std::unique_ptr<views::Widget> CreateWidget(TestWidgetDelegate* delegate) {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.delegate = delegate;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 100, 100);
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshTest);
};

// Verifies the client view is not placed at a y location of 0.
TEST_F(CustomFrameViewAshTest, ClientViewCorrectlyPlaced) {
  std::unique_ptr<views::Widget> widget(CreateWidget(new TestWidgetDelegate));
  widget->Show();
  EXPECT_NE(0, widget->client_view()->bounds().y());
}

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(CustomFrameViewAshTest, HeaderHeight) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate;

  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));
  widget->Show();

  // The header should have enough room for the window controls. The
  // header/content separator line overlays the window controls.
  EXPECT_EQ(
      GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).height(),
      delegate->custom_frame_view()->GetHeaderView()->height());
}

// Verify that CustomFrameViewAsh returns the correct minimum and maximum frame
// sizes when the client view does not specify any size constraints.
TEST_F(CustomFrameViewAshTest, NoSizeConstraints) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  gfx::Size min_frame_size = custom_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = custom_frame_view->GetMaximumSize();

  EXPECT_EQ(delegate->GetTitleBarHeight(), min_frame_size.height());

  // A width and height constraint of 0 denotes unbounded.
  EXPECT_EQ(0, max_frame_size.width());
  EXPECT_EQ(0, max_frame_size.height());
}

// Verify that CustomFrameViewAsh returns the correct minimum and maximum frame
// sizes when the client view specifies size constraints.
TEST_F(CustomFrameViewAshTest, MinimumAndMaximumSize) {
  gfx::Size min_client_size(500, 500);
  gfx::Size max_client_size(800, 800);
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  delegate->set_minimum_size(min_client_size);
  delegate->set_maximum_size(max_client_size);
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  gfx::Size min_frame_size = custom_frame_view->GetMinimumSize();
  gfx::Size max_frame_size = custom_frame_view->GetMaximumSize();

  EXPECT_EQ(min_client_size.width(), min_frame_size.width());
  EXPECT_EQ(max_client_size.width(), max_frame_size.width());
  EXPECT_EQ(min_client_size.height() + delegate->GetTitleBarHeight(),
            min_frame_size.height());
  EXPECT_EQ(max_client_size.height() + delegate->GetTitleBarHeight(),
            max_frame_size.height());
}

// Verify that CustomFrameViewAsh returns the correct minimum frame size when
// the kMinimumSize property is set.
TEST_F(CustomFrameViewAshTest, HonorsMinimumSizeProperty) {
  const gfx::Size min_client_size(500, 500);
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  delegate->set_minimum_size(min_client_size);
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  // Update the native window's minimum size property.
  const gfx::Size min_window_size(600, 700);
  widget->GetNativeWindow()->SetProperty(aura::client::kMinimumSize,
                                         new gfx::Size(min_window_size));

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  const gfx::Size min_frame_size = custom_frame_view->GetMinimumSize();

  EXPECT_EQ(min_window_size, min_frame_size);
}

// Verify that CustomFrameViewAsh updates the avatar icon based on the
// avatar icon window property.
TEST_F(CustomFrameViewAshTest, AvatarIcon) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  EXPECT_FALSE(custom_frame_view->GetAvatarIconViewForTest());

  // Avatar image becomes available.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kAvatarIconKey,
      new gfx::ImageSkia(gfx::test::CreateImage(27, 27).AsImageSkia()));
  EXPECT_TRUE(custom_frame_view->GetAvatarIconViewForTest());

  // Avatar image is gone; the ImageView for the avatar icon should be
  // removed.
  widget->GetNativeWindow()->ClearProperty(aura::client::kAvatarIconKey);
  EXPECT_FALSE(custom_frame_view->GetAvatarIconViewForTest());
}

// The visibility of the size button is updated when tablet mode is toggled.
// Verify that the layout of the HeaderView is updated for the size button's
// new visibility.
TEST_F(CustomFrameViewAshTest, HeaderViewNotifiedOfChildSizeChange) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  const gfx::Rect initial =
      delegate->GetFrameCaptionButtonContainerViewBounds();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  delegate->EndFrameCaptionButtonContainerViewAnimations();
  const gfx::Rect tablet_mode_bounds =
      delegate->GetFrameCaptionButtonContainerViewBounds();
  EXPECT_GT(initial.width(), tablet_mode_bounds.width());
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  delegate->EndFrameCaptionButtonContainerViewAnimations();
  const gfx::Rect after_restore =
      delegate->GetFrameCaptionButtonContainerViewBounds();
  EXPECT_EQ(initial, after_restore);
}

}  // namespace ash
