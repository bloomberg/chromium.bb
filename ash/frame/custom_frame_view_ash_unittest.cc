// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/custom_frame_view_ash.h"

#include <memory>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/header_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A views::WidgetDelegate which uses a CustomFrameViewAsh.
class CustomFrameTestWidgetDelegate : public views::WidgetDelegateView {
 public:
  CustomFrameTestWidgetDelegate() = default;
  ~CustomFrameTestWidgetDelegate() override = default;

  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    custom_frame_view_ = new CustomFrameViewAsh(widget);
    return custom_frame_view_;
  }

  int GetCustomFrameViewTopBorderHeight() {
    return custom_frame_view_->NonClientTopBorderHeight();
  }

  CustomFrameViewAsh* custom_frame_view() const { return custom_frame_view_; }

 private:
  // Not owned.
  CustomFrameViewAsh* custom_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameTestWidgetDelegate);
};

class TestWidgetConstraintsDelegate : public CustomFrameTestWidgetDelegate {
 public:
  TestWidgetConstraintsDelegate() = default;
  ~TestWidgetConstraintsDelegate() override = default;

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
  CustomFrameViewAshTest() = default;
  ~CustomFrameViewAshTest() override = default;

 protected:
  std::unique_ptr<views::Widget> CreateWidget(
      CustomFrameTestWidgetDelegate* delegate) {
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
  std::unique_ptr<views::Widget> widget(
      CreateWidget(new CustomFrameTestWidgetDelegate));
  widget->Show();
  EXPECT_NE(0, widget->client_view()->bounds().y());
}

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(CustomFrameViewAshTest, HeaderHeight) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;

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

// Verify that when in tablet mode with a maximized window, the height of the
// header is zero.
TEST_F(CustomFrameViewAshTest, FrameHiddenInTabletModeForMaximizedWindows) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));
  widget->Maximize();

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());
}

// Verify that when in tablet mode with a non maximized window, the height of
// the header is non zero.
TEST_F(CustomFrameViewAshTest, FrameShownInTabletModeForNonMaximizedWindows) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(
      GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).height(),
      delegate->GetCustomFrameViewTopBorderHeight());
}

// Verify that if originally in fullscreen mode, and enter tablet mode, the
// height of the header remains zero.
TEST_F(CustomFrameViewAshTest,
       FrameRemainsHiddenInTabletModeWhenTogglingFullscreen) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  std::unique_ptr<views::Widget> widget(CreateWidget(delegate));

  widget->SetFullscreen(true);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());
}

TEST_F(CustomFrameViewAshTest, OpeningAppsInTabletMode) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params;
  params.context = CurrentContext();
  params.delegate = delegate;
  widget->Init(params);
  widget->Show();
  widget->Maximize();

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());

  // Verify that after minimizing and showing the widget, the height of the
  // header is zero.
  widget->Minimize();
  widget->Show();
  widget->Maximize();
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());

  // Verify that when we toggle maximize, the header is shown. For example,
  // maximized can be toggled in tablet mode by using the accessibility
  // keyboard.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_MAXIMIZE);
  wm::GetWindowState(widget->GetNativeWindow())->OnWMEvent(&event);
  EXPECT_EQ(0, delegate->GetCustomFrameViewTopBorderHeight());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_EQ(
      GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).height(),
      delegate->GetCustomFrameViewTopBorderHeight());
}

namespace {

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() = default;
  ~TestTarget() override = default;

  size_t count() const { return count_; }

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    ++count_;
    return true;
  }

  bool CanHandleAccelerators() const override { return true; }

 private:
  size_t count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

}  // namespace

TEST_F(CustomFrameViewAshTest, BackButton) {
  ash::AcceleratorController* controller =
      ash::Shell::Get()->accelerator_controller();

  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params;
  params.context = CurrentContext();
  params.delegate = delegate;
  widget->Init(params);
  widget->Show();

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  TestTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  TestTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  HeaderView* header_view =
      static_cast<HeaderView*>(custom_frame_view->GetHeaderView());
  EXPECT_FALSE(header_view->back_button());
  custom_frame_view->SetBackButtonState(FrameBackButtonState::kVisibleDisabled);
  EXPECT_TRUE(header_view->back_button());
  EXPECT_FALSE(header_view->back_button()->enabled());

  // Back button is disabled, so clicking on it should not should
  // generate back key sequence.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(header_view->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(0u, target_back_press.count());
  EXPECT_EQ(0u, target_back_release.count());

  custom_frame_view->SetBackButtonState(FrameBackButtonState::kVisibleEnabled);
  EXPECT_TRUE(header_view->back_button());
  EXPECT_TRUE(header_view->back_button()->enabled());

  // Back button is now enabled, so clicking on it should generate
  // back key sequence.
  generator.MoveMouseTo(header_view->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(1u, target_back_press.count());
  EXPECT_EQ(1u, target_back_release.count());

  custom_frame_view->SetBackButtonState(FrameBackButtonState::kInvisible);
  EXPECT_FALSE(header_view->back_button());
}

// Make sure that client view occupies the entire window when the
// frame is hidden.
TEST_F(CustomFrameViewAshTest, FrameVisibility) {
  CustomFrameTestWidgetDelegate* delegate = new CustomFrameTestWidgetDelegate;
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(10, 10, 200, 100);
  params.context = CurrentContext();
  params.delegate = delegate;
  widget->Init(params);
  widget->Show();

  CustomFrameViewAsh* custom_frame_view = delegate->custom_frame_view();
  EXPECT_EQ(gfx::Size(200, 67), widget->client_view()->GetLocalBounds().size());

  custom_frame_view->SetVisible(false);
  widget->GetRootView()->Layout();
  EXPECT_EQ(gfx::Size(200, 100),
            widget->client_view()->GetLocalBounds().size());
  EXPECT_FALSE(widget->non_client_view()->frame_view()->visible());

  custom_frame_view->SetVisible(true);
  widget->GetRootView()->Layout();
  EXPECT_EQ(gfx::Size(200, 67), widget->client_view()->GetLocalBounds().size());
  EXPECT_TRUE(widget->non_client_view()->frame_view()->visible());
}

}  // namespace ash
