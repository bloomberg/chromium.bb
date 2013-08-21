// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "ash/ash_constants.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "grit/ash_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/non_client_view.h"

using ash::FramePainter;
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
      : is_paint_scheduled_(false) {
    window->AddObserver(this);
  }
  virtual ~WindowRepaintChecker() {
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
    window->RemoveObserver(this);
  }

  bool is_paint_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(WindowRepaintChecker);
};

// Modifies the values of kInactiveWindowOpacity, kActiveWindowOpacity, and
// kSoloWindowOpacity for the lifetime of the class. This is useful so that
// the constants each have different values.
class ScopedOpacityConstantModifier {
 public:
  ScopedOpacityConstantModifier()
      : initial_active_window_opacity_(
            ash::FramePainter::kActiveWindowOpacity),
        initial_inactive_window_opacity_(
            ash::FramePainter::kInactiveWindowOpacity),
        initial_solo_window_opacity_(ash::FramePainter::kSoloWindowOpacity) {
    ash::FramePainter::kActiveWindowOpacity = 100;
    ash::FramePainter::kInactiveWindowOpacity = 120;
    ash::FramePainter::kSoloWindowOpacity = 140;
  }
  ~ScopedOpacityConstantModifier() {
    ash::FramePainter::kActiveWindowOpacity = initial_active_window_opacity_;
    ash::FramePainter::kInactiveWindowOpacity =
        initial_inactive_window_opacity_;
    ash::FramePainter::kSoloWindowOpacity = initial_solo_window_opacity_;
  }

 private:
  int initial_active_window_opacity_;
  int initial_inactive_window_opacity_;
  int initial_solo_window_opacity_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOpacityConstantModifier);
};

// Creates a new FramePainter with empty buttons. Caller owns the memory.
FramePainter* CreateTestPainter(Widget* widget) {
  FramePainter* painter = new FramePainter();
  ImageButton* size_button = new ImageButton(NULL);
  ImageButton* close_button = new ImageButton(NULL);
  // Add the buttons to the widget's non-client frame view so they will be
  // deleted when the widget is destroyed.
  NonClientFrameView* frame_view = widget->non_client_view()->frame_view();
  frame_view->AddChildView(size_button);
  frame_view->AddChildView(close_button);
  painter->Init(widget,
                NULL,
                size_button,
                close_button,
                FramePainter::SIZE_BUTTON_MAXIMIZES);
  return painter;
}

// Self-owned manager of the frame painter which deletes the painter and itself
// when its widget is closed.
class FramePainterOwner : views::WidgetObserver {
 public:
  explicit FramePainterOwner(Widget* widget)
      : frame_painter_(CreateTestPainter(widget)) {
    widget->AddObserver(this);
  }

  virtual ~FramePainterOwner() {}

  FramePainter* frame_painter() { return frame_painter_.get(); }

 private:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE {
    widget->RemoveObserver(this);
    // Do not delete directly here, since the task of FramePainter causing
    // the crash of crbug.com/273310 may run after this class handles this
    // event.
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  scoped_ptr<FramePainter> frame_painter_;

  DISALLOW_COPY_AND_ASSIGN(FramePainterOwner);
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

  Widget* CreatePanelWidget() {
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    params.type = Widget::InitParams::TYPE_PANEL;
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

TEST_F(FramePainterTest, CreateAndDeleteSingleWindow) {
  // Ensure that creating/deleting a window works well and doesn't cause
  // crashes.  See crbug.com/155634
  aura::RootWindow* root = Shell::GetActiveRootWindow();

  scoped_ptr<Widget> widget(CreateTestWidget());
  scoped_ptr<FramePainter> painter(CreateTestPainter(widget.get()));
  widget->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(painter->UseSoloWindowHeader());
  EXPECT_TRUE(root->GetProperty(internal::kSoloWindowHeaderKey));

  // Close the window.
  widget.reset();
  EXPECT_FALSE(root->GetProperty(internal::kSoloWindowHeaderKey));

  // Recreate another window again.
  widget.reset(CreateTestWidget());
  painter.reset(CreateTestPainter(widget.get()));
  widget->Show();
  EXPECT_TRUE(painter->UseSoloWindowHeader());
  EXPECT_TRUE(root->GetProperty(internal::kSoloWindowHeaderKey));
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
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->Show();

  // Now there are two windows, so we should not use solo headers. This only
  // needs to test |p1| because "solo window headers" are a per-root-window
  // property.
  EXPECT_FALSE(p1->UseSoloWindowHeader());

  // Hide one window.  Solo should be enabled.
  w2->Hide();
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Show that window.  Solo should be disabled.
  w2->Show();
  EXPECT_FALSE(p1->UseSoloWindowHeader());

  // Minimize the second window.  Solo should be enabled.
  w2->Minimize();
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Close the minimized window.
  w2.reset();
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Open an always-on-top widget (which lives in a different container).
  scoped_ptr<Widget> w3(CreateAlwaysOnTopWidget());
  scoped_ptr<FramePainter> p3(CreateTestPainter(w3.get()));
  w3->Show();
  EXPECT_FALSE(p3->UseSoloWindowHeader());

  // Close the always-on-top widget.
  w3.reset();
  EXPECT_TRUE(p1->UseSoloWindowHeader());
}

// An open V2 app window should cause browser windows not to use the
// solo window header.
TEST_F(FramePainterTest, UseSoloWindowHeaderWithApp) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Simulate a V2 app window, which is part of the active workspace but does
  // not have a frame painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  w2->Show();

  // Now there are two windows, so we should not use solo headers.
  EXPECT_FALSE(p1->UseSoloWindowHeader());

  // Minimize the app window. The first window should go solo again.
  w2->Minimize();
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Restoring the app window turns off solo headers.
  w2->Restore();
  EXPECT_FALSE(p1->UseSoloWindowHeader());
}

// Panels should not "count" for computing solo window headers, and the panel
// itself should always have an opaque header.
TEST_F(FramePainterTest, UseSoloWindowHeaderWithPanel) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Create a panel and a painter for it.
  scoped_ptr<Widget> w2(CreatePanelWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because panels aren't included in the computation.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // The panel itself is not considered solo.
  EXPECT_FALSE(p2->UseSoloWindowHeader());

  // Even after closing the first window, the panel is still not considered
  // solo.
  w1.reset();
  EXPECT_FALSE(p2->UseSoloWindowHeader());
}

// Modal dialogs should not use solo headers.
TEST_F(FramePainterTest, UseSoloWindowHeaderModal) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Create a fake modal window.
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->GetNativeWindow()->SetProperty(aura::client::kModalKey,
                                     ui::MODAL_TYPE_WINDOW);
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because modal windows aren't included in the computation.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // The modal window itself is not considered solo.
  EXPECT_FALSE(p2->UseSoloWindowHeader());
}

// Constrained windows should not use solo headers.
TEST_F(FramePainterTest, UseSoloWindowHeaderConstrained) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // Create a fake constrained window.
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->GetNativeWindow()->SetProperty(ash::kConstrainedWindowKey, true);
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because constrained windows aren't included in the computation.
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  // The constrained window itself is not considered solo.
  EXPECT_FALSE(p2->UseSoloWindowHeader());
}

// Non-drawing windows should not affect the solo computation.
TEST_F(FramePainterTest, UseSoloWindowHeaderNotDrawn) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> widget(CreateTestWidget());
  scoped_ptr<FramePainter> painter(CreateTestPainter(widget.get()));
  widget->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_TRUE(painter->UseSoloWindowHeader());

  // Create non-drawing window similar to DragDropTracker.
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetDefaultParentByRootWindow(
      widget->GetNativeWindow()->GetRootWindow(), gfx::Rect());
  window->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because non-drawing windows aren't included in the computation.
  EXPECT_TRUE(painter->UseSoloWindowHeader());
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
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");

  // Create two widgets and painters for them.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->Show();
  WindowRepaintChecker checker1(w1->GetNativeWindow());
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->SetBounds(gfx::Rect(0, 0, 100, 100));
  w2->Show();
  WindowRepaintChecker checker2(w2->GetNativeWindow());

  // Now there are two windows in the same display, so we should not use solo
  // headers.
  EXPECT_FALSE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Moves the second window to the secondary display.  Both w1/w2 should be
  // solo.
  w2->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_TRUE(p1->UseSoloWindowHeader());
  EXPECT_TRUE(p2->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Open two more windows in the primary display.
  scoped_ptr<Widget> w3(CreateTestWidget());
  scoped_ptr<FramePainter> p3(CreateTestPainter(w3.get()));
  w3->SetBounds(gfx::Rect(0, 0, 100, 100));
  w3->Show();
  scoped_ptr<Widget> w4(CreateTestWidget());
  scoped_ptr<FramePainter> p4(CreateTestPainter(w4.get()));
  w4->SetBounds(gfx::Rect(0, 0, 100, 100));
  w4->Show();

  // Because the primary display has two windows w1 and w3, they shouldn't be
  // solo.  w2 should be solo.
  EXPECT_FALSE(p1->UseSoloWindowHeader());
  EXPECT_TRUE(p2->UseSoloWindowHeader());
  EXPECT_FALSE(p3->UseSoloWindowHeader());
  EXPECT_FALSE(p4->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Moves the w4 to the secondary display.  Now the w2 shouldn't be solo
  // anymore.
  w4->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_FALSE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());
  EXPECT_FALSE(p3->UseSoloWindowHeader());
  EXPECT_FALSE(p4->UseSoloWindowHeader());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Moves the w3 to the secondary display too.  Now w1 should be solo again.
  w3->SetBounds(gfx::Rect(1200, 0, 100, 100));
  EXPECT_TRUE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());
  EXPECT_FALSE(p3->UseSoloWindowHeader());
  EXPECT_FALSE(p4->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Change the w3 state to maximize.  Doesn't affect to w1.
  wm::MaximizeWindow(w3->GetNativeWindow());
  EXPECT_TRUE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());
  EXPECT_FALSE(p3->UseSoloWindowHeader());
  EXPECT_FALSE(p4->UseSoloWindowHeader());

  // Close the w3 and w4.
  w3.reset();
  w4.reset();
  EXPECT_TRUE(p1->UseSoloWindowHeader());
  EXPECT_TRUE(p2->UseSoloWindowHeader());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Move w2 back to the primary display.
  w2->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_FALSE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Close w2.
  w2.reset();
  EXPECT_TRUE(p1->UseSoloWindowHeader());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
}

TEST_F(FramePainterTest, GetHeaderOpacity) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // Modify the values of the opacity constants so that they each have a
  // different value.
  ScopedOpacityConstantModifier opacity_constant_modifier;

  // Solo active window has solo window opacity.
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Create a second widget and painter.
  scoped_ptr<Widget> w2(CreateTestWidget());
  scoped_ptr<FramePainter> p2(CreateTestPainter(w2.get()));
  w2->Show();

  // Active window has active window opacity.
  EXPECT_EQ(FramePainter::kActiveWindowOpacity,
            p2->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Inactive window has inactive window opacity.
  EXPECT_EQ(FramePainter::kInactiveWindowOpacity,
            p2->GetHeaderOpacity(FramePainter::INACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_INACTIVE,
                                 0));

  // Regular maximized windows are fully opaque.
  ash::wm::MaximizeWindow(w1->GetNativeWindow());
  EXPECT_EQ(255,
            p1->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));
}

// Test that the minimal header style is used in the proper situations.
TEST_F(FramePainterTest, MinimalHeaderStyle) {
  // Create a widget and a painter for it.
  scoped_ptr<Widget> w(CreateTestWidget());
  scoped_ptr<FramePainter> p(CreateTestPainter(w.get()));
  w->Show();

  // Regular non-maximized windows should not use the minimal header style.
  EXPECT_FALSE(p->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_NO));

  // Regular maximized windows should use the minimal header style.
  w->Maximize();
  EXPECT_TRUE(p->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_NO));

  // Test cases where the maximized window should not use the minimal header
  // style.
  EXPECT_FALSE(p->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_YES));

  SetTrackedByWorkspace(w->GetNativeWindow(), false);
  EXPECT_FALSE(p->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_NO));
  SetTrackedByWorkspace(w->GetNativeWindow(), true);
}

// Ensure the title text is vertically aligned with the window icon.
TEST_F(FramePainterTest, TitleIconAlignment) {
  scoped_ptr<Widget> w(CreateTestWidget());
  FramePainter p;
  ImageButton size(NULL);
  ImageButton close(NULL);
  views::View window_icon;
  window_icon.SetBounds(0, 0, 16, 16);
  p.Init(w.get(),
         &window_icon,
         &size,
         &close,
         FramePainter::SIZE_BUTTON_MAXIMIZES);
  w->SetBounds(gfx::Rect(0, 0, 500, 500));
  w->Show();

  // Title and icon are aligned when shorter_header is false.
  p.LayoutHeader(w->non_client_view()->frame_view(), false);
  gfx::Font default_font;
  gfx::Rect large_header_title_bounds = p.GetTitleBounds(default_font);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            large_header_title_bounds.CenterPoint().y());

  // Title and icon are aligned when shorter_header is true.
  p.LayoutHeader(w->non_client_view()->frame_view(), true);
  gfx::Rect short_header_title_bounds = p.GetTitleBounds(default_font);
  EXPECT_EQ(window_icon.bounds().CenterPoint().y(),
            short_header_title_bounds.CenterPoint().y());
}

TEST_F(FramePainterTest, ChildWindowVisibility) {
  scoped_ptr<Widget> w1(CreateTestWidget());
  scoped_ptr<FramePainter> p1(CreateTestPainter(w1.get()));
  w1->Show();

  // Solo active window has solo window opacity.
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Create a child window which doesn't affect the solo header.
  scoped_ptr<Widget> w2(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = w1->GetNativeView();
  w2->Init(params);
  w2->Show();

  // Still has solo header if child window is added.
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));

  // Change the visibility of w2 and verifies w1 still has solo header.
  w2->Hide();
  EXPECT_EQ(FramePainter::kSoloWindowOpacity,
            p1->GetHeaderOpacity(FramePainter::ACTIVE,
                                 IDR_AURA_WINDOW_HEADER_BASE_ACTIVE,
                                 0));
}

TEST_F(FramePainterTest, NoCrashShutdownWithAlwaysOnTopWindow) {
  // Create normal window and an always-on-top window, and leave it as is
  // and finish the test, then verify it doesn't cause a crash. See
  // crbug.com/273310.  Note that those widgets will be deleted at
  // RootWindowController::CloseChildWindows(), so this code is memory-safe.
  Widget* w1 = new Widget;
  Widget::InitParams params1;
  params1.context = CurrentContext();
  w1->Init(params1);
  FramePainterOwner* o1 = new FramePainterOwner(w1);
  FramePainter* p1 = o1->frame_painter();
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->Show();
  EXPECT_TRUE(p1->UseSoloWindowHeader());

  Widget* w2 = new Widget;
  Widget::InitParams params2;
  params2.context = CurrentContext();
  params2.keep_on_top = true;
  w2->Init(params2);
  FramePainterOwner* o2 = new FramePainterOwner(w2);
  FramePainter* p2 = o2->frame_painter();
  w2->Show();
  EXPECT_FALSE(p1->UseSoloWindowHeader());
  EXPECT_FALSE(p2->UseSoloWindowHeader());

  // Exit with no resource release. They'll be released at shutdown.
}

}  // namespace ash
