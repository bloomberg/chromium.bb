// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

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

  CustomFrameViewAsh* custom_frame_view() {
    return custom_frame_view_;
  }

 private:
  // Not owned.
  CustomFrameViewAsh* custom_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

typedef test::AshTestBase CustomFrameViewAshTest;

// Test that the height of the header is correct upon initially displaying
// the widget.
TEST_F(CustomFrameViewAshTest, HeaderHeight) {
  TestWidgetDelegate* delegate = new TestWidgetDelegate;

  scoped_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  params.context = CurrentContext();
  widget->Init(params);
  widget->Show();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* close_button =
      rb.GetImageSkiaNamed(IDR_AURA_WINDOW_CONTROL_BACKGROUND_H);

  // |kSeparatorSize| should match |kHeaderContentSeparatorSize| in
  // header_painter.cc
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

}  // namespace ash
