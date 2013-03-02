// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

using ui::ThemeProvider;
using views::Button;
using views::ImageButton;
using views::NonClientFrameView;
using views::ToggleImageButton;
using views::Widget;

namespace {

bool ImagesMatch(ImageButton* button,
                 int normal_image_id,
                 int hovered_image_id,
                 int pressed_image_id) {
  ThemeProvider* theme = button->GetWidget()->GetThemeProvider();
  gfx::ImageSkia* normal = theme->GetImageSkiaNamed(normal_image_id);
  gfx::ImageSkia* hovered = theme->GetImageSkiaNamed(hovered_image_id);
  gfx::ImageSkia* pressed = theme->GetImageSkiaNamed(pressed_image_id);
  return button->GetImage(Button::STATE_NORMAL).BackedBySameObjectAs(*normal) &&
      button->GetImage(Button::STATE_HOVERED).BackedBySameObjectAs(*hovered) &&
      button->GetImage(Button::STATE_PRESSED).BackedBySameObjectAs(*pressed);
}

class ResizableWidgetDelegate : public views::WidgetDelegate {
 public:
  ResizableWidgetDelegate(views::Widget* widget) {
    widget_ = widget;
  }

  virtual bool CanResize() const OVERRIDE { return true; }
  // Implementations of the widget class.
  virtual views::Widget* GetWidget() OVERRIDE { return widget_; }
  virtual const views::Widget* GetWidget() const OVERRIDE { return widget_; }
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ResizableWidgetDelegate);
};

class WindowRepaintChecker : public aura::WindowObserver {
 public:
  explicit WindowRepaintChecker(aura::Window* window)
      : window_(window),
        is_paint_scheduled_(false) {
    window_->AddObserver(this);
  }
  virtual ~WindowRepaintChecker() {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool IsPaintScheduledAndReset() {
    bool result = is_paint_scheduled_;
    is_paint_scheduled_ = false;
    return result;
  }

 private:
  // aura::WindowObserver overrides:
  virtual void OnWindowPaintScheduled(aura::Window* window,
                                      const gfx::Rect& region) OVERRIDE {
    is_paint_scheduled_ = true;
  }
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window_, window);
    window_ = NULL;
    window->RemoveObserver(this);
  }

  aura::Window* window_;
  bool is_paint_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(WindowRepaintChecker);
};

}  // namespace

namespace ash {

class FramePainterTest : public ash::test::AshTestBase {
 public:
  // Creates a test widget that owns its native widget.
  Widget* CreateTestWidget() {
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }

  Widget* CreateAlwaysOnTopWidget() {
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    params.keep_on_top = true;
    widget->Init(params);
    return widget;
  }

  Widget* CreateResizableWidget() {
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    params.keep_on_top = true;
    params.delegate = new ResizableWidgetDelegate(widget);
    params.type = Widget::InitParams::TYPE_WINDOW;
    widget->Init(params);
    return widget;
  }
};

TEST_F(FramePainterTest, Basics) {
  // Other tests might have created a FramePainter, so we cannot assert that
  // FramePainter::instances_ is NULL here.

  // Creating a painter bumps the instance count.
  scoped_ptr<FramePainter> painter(new FramePainter);
  ASSERT_TRUE(FramePainter::instances_);
  EXPECT_EQ(1u, FramePainter::instances_->size());

  // Destroying that painter leaves a valid pointer but no instances.
  painter.reset();
  ASSERT_TRUE(FramePainter::instances_);
  EXPECT_EQ(0u, FramePainter::instances_->size());
}

TEST_F(FramePainterTest, CreateAndDeleteSingleWindow) {
  // Ensure that creating/deleting a window works well and doesn't cause
  // crashes.  See crbug.com/155634
  aura::RootWindow* root = Shell::GetActiveRootWindow();

  scoped_ptr<Widget> widget(CreateTestWidget());
  scoped_ptr<FramePainter> painter(new FramePainter);
  ImageButton size(NULL);
  ImageButton close(NULL);
  painter->Init(
      widget.get(), NULL, &size, &close, FramePainter::SIZE_BUTTON_MAXIMIZES);
  widget->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(painter->UseSoloWindowHeader());
  EXPECT_EQ(painter.get(),
            root->GetProperty(internal::kSoloWindowFramePainterKey));

  // Close the window.
  widget.reset();
  EXPECT_EQ(NULL, root->GetProperty(internal::kSoloWindowFramePainterKey));

  // Recreate another window again.
  painter.reset(new FramePainter);
  widget.reset(CreateTestWidget());

  painter->Init(
      widget.get(), NULL, &size, &close, FramePainter::SIZE_BUTTON_MAXIMIZES);
  widget->Show();
  EXPECT_TRUE(painter->UseSoloWindowHeader());
  EXPECT_EQ(painter.get(),
            root->GetProperty(internal::kSoloWindowFramePainterKey));
}

TEST_F(FramePainterTest, LayoutHeader) {
  scoped_ptr<Widget> widget(CreateTestWidget());
  ImageButton size_button(NULL);
  ImageButton close_button(NULL);
  NonClientFrameView* frame_view = widget->non_client_view()->frame_view();
  frame_view->AddChildView(&size_button);
  frame_view->AddChildView(&close_button);
  scoped_ptr<FramePainter> painter(new FramePainter);
  painter->Init(widget.get(),
                NULL,
                &size_button,
                &close_button,
                FramePainter::SIZE_BUTTON_MAXIMIZES);
  widget->Show();

  // Basic layout.
  painter->LayoutHeader(frame_view, false);
  EXPECT_TRUE(ImagesMatch(&close_button,
                          IDR_AURA_WINDOW_CLOSE,
                          IDR_AURA_WINDOW_CLOSE_H,
                          IDR_AURA_WINDOW_CLOSE_P));
  EXPECT_TRUE(ImagesMatch(&size_button,
                          IDR_AURA_WINDOW_MAXIMIZE,
                          IDR_AURA_WINDOW_MAXIMIZE_H,
                          IDR_AURA_WINDOW_MAXIMIZE_P));

  // Shorter layout.
  painter->LayoutHeader(frame_view, true);
  EXPECT_TRUE(ImagesMatch(&close_button,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P));
  EXPECT_TRUE(ImagesMatch(&size_button,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P));

  // Maximized shorter layout.
  widget->Maximize();
  painter->LayoutHeader(frame_view, true);
  EXPECT_TRUE(ImagesMatch(&close_button,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P));
  EXPECT_TRUE(ImagesMatch(&size_button,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P));

  // Fullscreen can show the buttons during an immersive reveal, so it should
  // use the same images as maximized.
  widget->SetFullscreen(true);
  painter->LayoutHeader(frame_view, true);
  EXPECT_TRUE(ImagesMatch(&close_button,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
                          IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P));
  EXPECT_TRUE(ImagesMatch(&size_button,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
                          IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P));
}

TEST_F(FramePainterTest, UseSoloWindowHeader) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1.UseSoloWindowHeader());

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  FramePainter p2;
  ImageButton size2(NULL);
  ImageButton close2(NULL);
  p2.Init(w2.get(), NULL, &size2, &close2, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w2->Show();

  // Now there are two windows, so we should not use solo headers.
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());

  // Hide one window.  Solo should be enabled.
  w2->Hide();
  EXPECT_TRUE(p1.UseSoloWindowHeader());

  // Show that window.  Solo should be disabled.
  w2->Show();
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());

  // Maximize the window, then activate the first window. The second window
  // is in its own workspace, so solo should be active for the first one.
  w2->Maximize();
  w1->Activate();
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());

  // Switch to the second window and restore it.  Solo should be disabled.
  w2->Activate();
  w2->Restore();
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());

  // Minimize the second window.  Solo should be enabled.
  w2->Minimize();
  EXPECT_TRUE(p1.UseSoloWindowHeader());

  // Close the minimized window.
  w2.reset();
  EXPECT_TRUE(p1.UseSoloWindowHeader());

  // Open an always-on-top widget (which lives in a different container).
  scoped_ptr<Widget> w3(CreateAlwaysOnTopWidget());
  FramePainter p3;
  ImageButton size3(NULL);
  ImageButton close3(NULL);
  p3.Init(w3.get(), NULL, &size3, &close3, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w3->Show();
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p3.UseSoloWindowHeader());

  // Close the always-on-top widget.
  w3.reset();
  EXPECT_TRUE(p1.UseSoloWindowHeader());
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_UseSoloWindowHeaderMultiDisplay \
        DISABLED_UseSoloWindowHeaderMultiDisplay
#else
#define MAYBE_UseSoloWindowHeaderMultiDisplay \
        UseSoloWindowHeaderMultiDisplay
#endif

TEST_F(FramePainterTest, MAYBE_UseSoloWindowHeaderMultiDisplay) {
  UpdateDisplay("1000x600,600x400");

  // Create two widgets and painters for them.
  scoped_ptr<Widget> w1(CreateTestWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->Show();
  WindowRepaintChecker checker1(w1->GetNativeWindow());
  scoped_ptr<Widget> w2(CreateTestWidget());
  FramePainter p2;
  ImageButton size2(NULL);
  ImageButton close2(NULL);
  p2.Init(w2.get(), NULL, &size2, &close2, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w2->SetBounds(gfx::Rect(0, 0, 100, 100));
  w2->Show();
  WindowRepaintChecker checker2(w2->GetNativeWindow());

  // Now there are two windows in the same display, so we should not use solo
  // headers.
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Moves the second window to the secondary display.  Both w1/w2 should be
  // solo.
  w2->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_TRUE(p2.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Open two more windows in the primary display.
  scoped_ptr<Widget> w3(CreateTestWidget());
  FramePainter p3;
  ImageButton size3(NULL);
  ImageButton close3(NULL);
  p3.Init(w3.get(), NULL, &size3, &close3, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w3->SetBounds(gfx::Rect(0, 0, 100, 100));
  w3->Show();
  scoped_ptr<Widget> w4(CreateTestWidget());
  FramePainter p4;
  ImageButton size4(NULL);
  ImageButton close4(NULL);
  p4.Init(w4.get(), NULL, &size4, &close4, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w4->SetBounds(gfx::Rect(0, 0, 100, 100));
  w4->Show();

  // Because the primary display has two windows w1 and w3, they shouldn't be
  // solo.  w2 should be solo.
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_TRUE(p2.UseSoloWindowHeader());
  EXPECT_FALSE(p3.UseSoloWindowHeader());
  EXPECT_FALSE(p4.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Moves the w4 to the secondary display.  Now the w2 shouldn't be solo
  // anymore.
  w4->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());
  EXPECT_FALSE(p3.UseSoloWindowHeader());
  EXPECT_FALSE(p4.UseSoloWindowHeader());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Moves the w3 to the secondary display too.  Now w1 should be solo again.
  w3->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());
  EXPECT_FALSE(p3.UseSoloWindowHeader());
  EXPECT_FALSE(p4.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Change the w3 state to maximize.  Doesn't affect to w1.
  wm::MaximizeWindow(w3->GetNativeWindow());
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());
  EXPECT_FALSE(p3.UseSoloWindowHeader());
  EXPECT_FALSE(p4.UseSoloWindowHeader());

  // Close the w3 and w4.
  w3.reset();
  w4.reset();
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_TRUE(p2.UseSoloWindowHeader());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Move w2 back to the primary display.
  w2->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_FALSE(p1.UseSoloWindowHeader());
  EXPECT_FALSE(p2.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Close w2.
  w2.reset();
  EXPECT_TRUE(p1.UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
}

TEST_F(FramePainterTest, GetHeaderOpacity) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w1->Show();

  // Solo active window has solo window opacity.
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                NULL));

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  FramePainter p2;
  ImageButton size2(NULL);
  ImageButton close2(NULL);
  p2.Init(w2.get(), NULL, &size2, &close2, FramePainter::SIZE_BUTTON_MAXIMIZES);
  w2->Show();

  // Active window has active window opacity.
  EXPECT_EQ(FramePainter::kActiveWindowOpacity,
            p2.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                NULL));

  // Inactive window has inactive window opacity.
  EXPECT_EQ(FramePainter::kInactiveWindowOpacity,
            p2.GetHeaderOpacity(FramePainter::INACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_INACTIVE,
                                NULL));

  // Custom overlay image is drawn completely opaque.
  gfx::ImageSkia custom_overlay;
  EXPECT_EQ(255,
            p1.GetHeaderOpacity(FramePainter::ACTIVE,
                                IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                &custom_overlay));
}

// Test the hit test function with windows which are "partially maximized".
TEST_F(FramePainterTest, HitTestSpecialMaximizedModes) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateResizableWidget());
  FramePainter p1;
  ImageButton size1(NULL);
  ImageButton close1(NULL);
  p1.Init(w1.get(), NULL, &size1, &close1, FramePainter::SIZE_BUTTON_MAXIMIZES);
  views::NonClientFrameView* frame = w1->non_client_view()->frame_view();
  w1->Show();
  gfx::Rect any_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect screen = Shell::GetScreen()->GetDisplayMatching(
      any_rect).work_area();
  w1->SetBounds(any_rect);
  EXPECT_EQ(HTTOPLEFT, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  w1->SetBounds(gfx::Rect(
      screen.x(), screen.y(), screen.width() / 2, screen.height()));
  // A hit without a set restore rect should produce a top left hit.
  EXPECT_EQ(HTTOPLEFT, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  ash::SetRestoreBoundsInScreen(w1->GetNativeWindow(), any_rect);
  // A hit into the corner should produce nowhere - not left.
  EXPECT_EQ(HTCAPTION, p1.NonClientHitTest(frame, gfx::Point(0, 15)));
  // A hit into the middle upper area should generate right - not top&right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2, 15)));
  // A hit into the middle should generate right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2,
                                                  screen.height() / 2)));
  // A hit into the middle lower area should generate right - not bottom&right.
  EXPECT_EQ(HTRIGHT,
            p1.NonClientHitTest(frame, gfx::Point(screen.width() / 2,
                                                  screen.height() - 1)));
}

}  // namespace ash
