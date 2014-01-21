// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"

#include "chrome/browser/ui/views/apps/native_app_window_views.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"

class ShapedAppWindowTargeterTest : public aura::test::AuraTestBase {
 public:
  ShapedAppWindowTargeterTest()
      : web_view_(NULL) {
  }

  virtual ~ShapedAppWindowTargeterTest() {}

  views::Widget* widget() { return widget_.get(); }

  apps::NativeAppWindow* app_window() { return &app_window_; }
  NativeAppWindowViews* app_window_views() { return &app_window_; }

 protected:
  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    widget_.reset(new views::Widget);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.remove_standard_frame = true;
    params.bounds = gfx::Rect(30, 30, 100, 100);
    params.context = root_window();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);

    app_window_.web_view_ = &web_view_;
    app_window_.window_ = widget_.get();

    widget_->Show();
  }

  virtual void TearDown() OVERRIDE {
    widget_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 private:
  views::WebView web_view_;
  scoped_ptr<views::Widget> widget_;
  NativeAppWindowViews app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShapedAppWindowTargeterTest);
};

TEST_F(ShapedAppWindowTargeterTest, HitTestBasic) {
  aura::Window* window = widget()->GetNativeWindow();
  window->set_event_targeter(scoped_ptr<ui::EventTargeter>(
      new ShapedAppWindowTargeter(window, app_window_views())));
  {
    // Without any custom shapes, the event should be targeted correctly to the
    // window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(40, 40), gfx::Point(40, 40),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

  scoped_ptr<SkRegion> region(new SkRegion);
  region->op(SkIRect::MakeXYWH(40, 0, 20, 100), SkRegion::kUnion_Op);
  region->op(SkIRect::MakeXYWH(0, 40, 100, 20), SkRegion::kUnion_Op);
  app_window()->UpdateShape(region.Pass());
  {
    // With the custom shape, the events that don't fall within the custom shape
    // will go through to the root window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(40, 40), gfx::Point(40, 40),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());

    // But events within the shape will still reach the window.
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED,
                         gfx::Point(80, 80), gfx::Point(80, 80),
                         ui::EF_NONE, ui::EF_NONE);
    details = dispatcher()->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move2.target());
  }
}
