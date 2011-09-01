// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"
#include "aura/window_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas_skia.h"

namespace aura {
namespace internal {

namespace {

// A simple WindowDelegate implementation for these tests. It owns itself
// (deletes itself when the Window it is attached to is destroyed).
class TestWindowDelegate : public WindowDelegate {
 public:
  TestWindowDelegate(SkColor color) : color_(color) {}
  virtual ~TestWindowDelegate() {}

  // Overridden from WindowDelegate:
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE { return false; }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->AsCanvasSkia()->drawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class WindowTest : public testing::Test {
 public:
  WindowTest() {
    aura::Desktop::GetInstance()->Show();
    aura::Desktop::GetInstance()->SetSize(gfx::Size(500, 500));
  }
  virtual ~WindowTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
  }

  virtual void TearDown() OVERRIDE {
  }

  Window* CreateTestWindow(SkColor color,
                           int id,
                           const gfx::Rect& bounds,
                           Window* parent) {
    Window* window = new Window(new TestWindowDelegate(color));
    window->set_id(id);
    window->Init();
    window->SetBounds(bounds, 0);
    window->SetVisibility(Window::VISIBILITY_SHOWN);
    window->SetParent(parent);
    return window;
  }

  void RunPendingMessages() {
    MessageLoop main_message_loop(MessageLoop::TYPE_UI);
    MessageLoopForUI::current()->Run(NULL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowTest);
};

}  // namespace

TEST_F(WindowTest, HitTest) {
  Window w1(new TestWindowDelegate(SK_ColorWHITE));
  w1.set_id(1);
  w1.Init();
  w1.SetBounds(gfx::Rect(10, 10, 50, 50), 0);
  w1.SetVisibility(Window::VISIBILITY_SHOWN);
  w1.SetParent(NULL);

  // Points are in the Window's coordinates.
  EXPECT_TRUE(w1.HitTest(gfx::Point(1, 1)));
  EXPECT_FALSE(w1.HitTest(gfx::Point(-1, -1)));

  // TODO(beng): clip Window to parent.
}

TEST_F(WindowTest, GetEventHandlerForPoint) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), NULL));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<Window> w12(
      CreateTestWindow(SK_ColorMAGENTA, 12, gfx::Rect(10, 420, 25, 25),
                       w1.get()));
  scoped_ptr<Window> w121(
      CreateTestWindow(SK_ColorYELLOW, 121, gfx::Rect(5, 5, 5, 5), w12.get()));
  scoped_ptr<Window> w13(
      CreateTestWindow(SK_ColorGRAY, 13, gfx::Rect(5, 470, 50, 50), w1.get()));

  Window* desktop = Desktop::GetInstance()->window();
  EXPECT_EQ(desktop, desktop->GetEventHandlerForPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w1.get(), desktop->GetEventHandlerForPoint(gfx::Point(11, 11)));
  EXPECT_EQ(w11.get(), desktop->GetEventHandlerForPoint(gfx::Point(16, 16)));
  EXPECT_EQ(w111.get(), desktop->GetEventHandlerForPoint(gfx::Point(21, 21)));
  EXPECT_EQ(w1111.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 26)));
  EXPECT_EQ(w12.get(), desktop->GetEventHandlerForPoint(gfx::Point(21, 431)));
  EXPECT_EQ(w121.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 436)));
  EXPECT_EQ(w13.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 481)));
}

}  // namespace internal
}  // namespace aura

