// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/custom_frame_view_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A views::WidgetDelegate which uses a CustomFrameViewAsh.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {
  }
  virtual ~TestWidgetDelegate() {
  }

  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    custom_frame_view_ = new CustomFrameViewAsh(widget);
    return custom_frame_view_;
  }

  CustomFrameViewAsh* custom_frame_view() const {
    return custom_frame_view_;
  }

 private:
  // Not owned.
  CustomFrameViewAsh* custom_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

class TestWidgetConstraintsDelegate : public TestWidgetDelegate {
 public:
  TestWidgetConstraintsDelegate() {
  }
  virtual ~TestWidgetConstraintsDelegate() {
  }

  // views::View implementation.
  virtual gfx::Size GetMinimumSize() OVERRIDE {
    return minimum_size_;
  }

  virtual gfx::Size GetMaximumSize() OVERRIDE {
    return maximum_size_;
  }

  virtual views::View* GetContentsView() OVERRIDE {
    // Set this instance as the contents view so that the maximum and minimum
    // size constraints will be used.
    return this;
  }

  void set_minimum_size(const gfx::Size& min_size) {
    minimum_size_ = min_size;
  }

  void set_maximum_size(const gfx::Size& max_size) {
    maximum_size_ = max_size;
  }

  int GetTitleBarHeight() const {
    return custom_frame_view()->NonClientTopBorderHeight();
  }

 private:
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetConstraintsDelegate);
};

class CustomFrameViewAshTest : public test::AshTestBase {
 public:
  CustomFrameViewAshTest() {}
  virtual ~CustomFrameViewAshTest() {}

 protected:
  scoped_ptr<views::Widget> CreateWidget(TestWidgetDelegate* delegate) {
    scoped_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.delegate = delegate;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 100, 100);
    params.context = CurrentContext();
    widget->Init(params);
    return widget.Pass();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshTest);
};

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(CustomFrameViewAshTest, HeaderHeight) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate;

  scoped_ptr<views::Widget> widget(CreateWidget(delegate));
  widget->Show();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* close_button =
      rb.GetImageSkiaNamed(IDR_AURA_WINDOW_CONTROL_BACKGROUND_H);

  // |kSeparatorSize| should match |kHeaderContentSeparatorSize| in
  // default_header_painter.cc
  // TODO(pkotwicz): Clean this test up once the separator overlays the window
  // controls.
  const int kSeparatorSize = 1;

  // The header should have enough room for the window controls and the
  // separator line.
  EXPECT_EQ(close_button->height() + kSeparatorSize,
            delegate->custom_frame_view()->GetHeaderView()->height());

  widget->Maximize();
  EXPECT_EQ(close_button->height() + kSeparatorSize,
            delegate->custom_frame_view()->GetHeaderView()->height());
}

// Verify that CustomFrameViewAsh returns the correct minimum and maximum frame
// sizes when the client view does not specify any size constraints.
TEST_F(CustomFrameViewAshTest, NoSizeConstraints) {
  TestWidgetConstraintsDelegate* delegate = new TestWidgetConstraintsDelegate;
  scoped_ptr<views::Widget> widget(CreateWidget(delegate));

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
  scoped_ptr<views::Widget> widget(CreateWidget(delegate));

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

}  // namespace ash
