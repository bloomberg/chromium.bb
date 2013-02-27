// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_activation_delegate.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_event_handler.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/input_method_event_filter.h"

namespace {

base::TimeDelta getTime() {
  return ui::EventTimeForNow();
}

// A slightly changed TestEventHandler which can be configured to return a
// specified value for key/mouse event handling.
class CustomEventHandler : public aura::test::TestEventHandler {
 public:
  CustomEventHandler()
      : key_result_(ui::ER_UNHANDLED),
        mouse_result_(ui::ER_UNHANDLED) {
  }

  virtual ~CustomEventHandler() {}

  void set_key_event_handling_result(ui::EventResult result) {
    key_result_ = result;
  }

  void set_mouse_event_handling_result(ui::EventResult result) {
    mouse_result_ = result;
  }

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    aura::test::TestEventHandler::OnKeyEvent(event);
    if (key_result_ & ui::ER_HANDLED)
      event->SetHandled();
    if (key_result_ & ui::ER_CONSUMED)
      event->StopPropagation();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    aura::test::TestEventHandler::OnMouseEvent(event);
    if (mouse_result_ & ui::ER_HANDLED)
      event->SetHandled();
    if (mouse_result_ & ui::ER_CONSUMED)
      event->StopPropagation();
  }

 private:
  ui::EventResult key_result_;
  ui::EventResult mouse_result_;

  DISALLOW_COPY_AND_ASSIGN(CustomEventHandler);
};

}  // namespace

namespace ash {

typedef test::AshTestBase WindowManagerTest;

class NonFocusableDelegate : public aura::test::TestWindowDelegate {
 public:
  NonFocusableDelegate() {}

 private:
  virtual bool CanFocus() OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(NonFocusableDelegate);
};

class HitTestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  HitTestWindowDelegate()
      : hittest_code_(HTNOWHERE) {
  }
  virtual ~HitTestWindowDelegate() {}
  void set_hittest_code(int hittest_code) { hittest_code_ = hittest_code; }

 private:
  // Overridden from TestWindowDelegate:
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return hittest_code_;
  }

  int hittest_code_;

  DISALLOW_COPY_AND_ASSIGN(HitTestWindowDelegate);
};

TEST_F(WindowManagerTest, Focus) {
  // The IME event filter interferes with the basic key event propagation we
  // attempt to do here, so we remove it.
  test::ShellTestApi shell_test(Shell::GetInstance());
  Shell::GetInstance()->RemovePreTargetHandler(
      shell_test.input_method_event_filter());

  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  root_window->SetBounds(gfx::Rect(0, 0, 510, 510));

  // Supplied ids are negative so as not to collide with shell ids.
  // TODO(beng): maybe introduce a MAKE_SHELL_ID() macro that generates a safe
  //             id beyond shell id max?
  scoped_ptr<aura::Window> w1(CreateTestWindowInShell(
      SK_ColorWHITE, -1, gfx::Rect(10, 10, 500, 500)));
  scoped_ptr<aura::Window> w11(aura::test::CreateTestWindow(
      SK_ColorGREEN, -11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<aura::Window> w111(aura::test::CreateTestWindow(
      SK_ColorCYAN, -111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<aura::Window> w1111(aura::test::CreateTestWindow(
      SK_ColorRED, -1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<aura::Window> w12(aura::test::CreateTestWindow(
      SK_ColorMAGENTA, -12, gfx::Rect(10, 420, 25, 25), w1.get()));
  aura::test::ColorTestWindowDelegate* w121delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorYELLOW);
  scoped_ptr<aura::Window> w121(aura::test::CreateTestWindowWithDelegate(
      w121delegate, -121, gfx::Rect(5, 5, 5, 5), w12.get()));
  aura::test::ColorTestWindowDelegate* w122delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorRED);
  scoped_ptr<aura::Window> w122(aura::test::CreateTestWindowWithDelegate(
      w122delegate, -122, gfx::Rect(10, 5, 5, 5), w12.get()));
  aura::test::ColorTestWindowDelegate* w123delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorRED);
  scoped_ptr<aura::Window> w123(aura::test::CreateTestWindowWithDelegate(
      w123delegate, -123, gfx::Rect(15, 5, 5, 5), w12.get()));
  scoped_ptr<aura::Window> w13(aura::test::CreateTestWindow(
      SK_ColorGRAY, -13, gfx::Rect(5, 470, 50, 50), w1.get()));

  // Click on a sub-window (w121) to focus it.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       w121.get());
  generator.ClickLeftButton();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w121.get());
  EXPECT_EQ(w121.get(), focus_client->GetFocusedWindow());

  // The key press should be sent to the focused sub-window.
  ui::KeyEvent keyev(ui::ET_KEY_PRESSED, ui::VKEY_E, 0, false);
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&keyev);
  EXPECT_EQ(ui::VKEY_E, w121delegate->last_key_code());

  // Touch on a sub-window (w122) to focus it.
  gfx::Point click_point = w122->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w122->parent(), root_window, &click_point);
  ui::TouchEvent touchev(ui::ET_TOUCH_PRESSED, click_point, 0, getTime());
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&touchev);
  focus_client = aura::client::GetFocusClient(w122.get());
  EXPECT_EQ(w122.get(), focus_client->GetFocusedWindow());

  // The key press should be sent to the focused sub-window.
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&keyev);
  EXPECT_EQ(ui::VKEY_E, w122delegate->last_key_code());

  // Hiding the focused window will set the focus to its parent if
  // it's focusable.
  w122->Hide();
  EXPECT_EQ(aura::client::GetFocusClient(w12.get()),
            aura::client::GetFocusClient(w122.get()));
  EXPECT_EQ(w12.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Sets the focus back to w122.
  w122->Show();
  w122->Focus();
  EXPECT_EQ(w122.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Removing the focused window from parent should set the focus to
  // its parent if it's focusable.
  w12->RemoveChild(w122.get());
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w122.get()));
  EXPECT_EQ(w12.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());

  // Set the focus to w123, but make the w1 not activatable.
  test::TestActivationDelegate activation_delegate(false);
  w123->Focus();
  EXPECT_EQ(w123.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  aura::client::SetActivationDelegate(w1.get(), &activation_delegate);

  // Hiding the focused window will set the focus to NULL because
  // parent window is not focusable.
  w123->Hide();
  EXPECT_EQ(aura::client::GetFocusClient(w12.get()),
            aura::client::GetFocusClient(w123.get()));
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  EXPECT_FALSE(root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&keyev));

  // Set the focus back to w123
  aura::client::SetActivationDelegate(w1.get(), NULL);
  w123->Show();
  w123->Focus();
  EXPECT_EQ(w123.get(),
            aura::client::GetFocusClient(w12.get())->GetFocusedWindow());
  aura::client::SetActivationDelegate(w1.get(), &activation_delegate);

  // Removing the focused window will set the focus to NULL because
  // parent window is not focusable.
  w12->RemoveChild(w123.get());
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w123.get()));
  EXPECT_FALSE(root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&keyev));
}

// Various assertion testing for activating windows.
TEST_F(WindowManagerTest, ActivateOnMouse) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  test::TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &wd, -1, gfx::Rect(10, 10, 50, 50)));
  d1.SetWindow(w1.get());
  test::TestActivationDelegate d2;
  scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &wd, -2, gfx::Rect(70, 70, 50, 50)));
  d2.SetWindow(w2.get());

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w1.get());

  d1.Clear();
  d2.Clear();

  // Activate window1.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  {
    // Click on window2.
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         w2.get());
    generator.ClickLeftButton();

    // Window2 should have become active.
    EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(0, d1.activated_count());
    EXPECT_EQ(1, d1.lost_active_count());
    EXPECT_EQ(1, d2.activated_count());
    EXPECT_EQ(0, d2.lost_active_count());
    d1.Clear();
    d2.Clear();
  }

  {
    // Click back on window1, but set it up so w1 doesn't activate on click.
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         w1.get());
    d1.set_activate(false);
    generator.ClickLeftButton();

    // Window2 should still be active and focused.
    EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(0, d1.activated_count());
    EXPECT_EQ(0, d1.lost_active_count());
    EXPECT_EQ(0, d2.activated_count());
    EXPECT_EQ(0, d2.lost_active_count());
    d1.Clear();
    d2.Clear();
  }

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(views::corewm::UseFocusController() ? 1 : 0,
            d2.lost_active_count());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());

  // Clicking an active window with a child shouldn't steal the
  // focus from the child.
  {
    scoped_ptr<aura::Window> w11(CreateTestWindowWithDelegate(
          &wd, -11, gfx::Rect(10, 10, 10, 10), w1.get()));
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         w11.get());
    // First set the focus to the child |w11|.
    generator.ClickLeftButton();
    EXPECT_EQ(w11.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(w1.get(), wm::GetActiveWindow());

    // Then click the parent active window. The focus shouldn't move.
    gfx::Point left_top = w1->bounds().origin();
    aura::Window::ConvertPointToTarget(w1->parent(), root_window, &left_top);
    left_top.Offset(1, 1);
    generator.MoveMouseTo(left_top);
    generator.ClickLeftButton();
    EXPECT_EQ(w11.get(), focus_client->GetFocusedWindow());
    EXPECT_EQ(w1.get(), wm::GetActiveWindow());
  }

  // Clicking on a non-focusable window inside a background window should still
  // give focus to the background window.
  {
    NonFocusableDelegate nfd;
    scoped_ptr<aura::Window> w11(CreateTestWindowWithDelegate(
          &nfd, -1, gfx::Rect(10, 10, 10, 10), w1.get()));
    // Move focus to |w2| first.
    scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
          &wd, -1, gfx::Rect(70, 70, 50, 50)));
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         w2.get());
    generator.ClickLeftButton();
    EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
    EXPECT_FALSE(w11->CanFocus());

    // Click on |w11|. This should focus w1.
    generator.MoveMouseToCenterOf(w11.get());
    generator.ClickLeftButton();
    EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  }
}

// Essentially the same as ActivateOnMouse, but for touch events.
TEST_F(WindowManagerTest, ActivateOnTouch) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  test::TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &wd, -1, gfx::Rect(10, 10, 50, 50)));
  d1.SetWindow(w1.get());
  test::TestActivationDelegate d2;
  scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &wd, -2, gfx::Rect(70, 70, 50, 50)));
  d2.SetWindow(w2.get());

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(w1.get());

  d1.Clear();
  d2.Clear();

  // Activate window1.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  // Touch window2.
  gfx::Point press_point = w2->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w2->parent(), root_window, &press_point);
  ui::TouchEvent touchev1(ui::ET_TOUCH_PRESSED, press_point, 0, getTime());
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&touchev1);

  // Window2 should have become active.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(1, d1.lost_active_count());
  EXPECT_EQ(1, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Touch window1, but set it up so w1 doesn't activate on touch.
  press_point = w1->bounds().CenterPoint();
  aura::Window::ConvertPointToTarget(w1->parent(), root_window, &press_point);
  d1.set_activate(false);
  ui::TouchEvent touchev2(ui::ET_TOUCH_PRESSED, press_point, 1, getTime());
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&touchev2);

  // Window2 should still be active and focused.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(w2.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(views::corewm::UseFocusController() ? 1 : 0,
            d2.lost_active_count());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(), focus_client->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
}

TEST_F(WindowManagerTest, MouseEventCursors) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  // Create a window.
  const int kWindowLeft = 123;
  const int kWindowTop = 45;
  HitTestWindowDelegate window_delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
    &window_delegate,
    -1,
    gfx::Rect(kWindowLeft, kWindowTop, 640, 480)));

  // Create two mouse movement events we can switch between.
  gfx::Point point1(kWindowLeft, kWindowTop);
  aura::Window::ConvertPointToTarget(window->parent(), root_window, &point1);

  gfx::Point point2(kWindowLeft + 1, kWindowTop + 1);
  aura::Window::ConvertPointToTarget(window->parent(), root_window, &point2);

  // Cursor starts as a pointer (set during Shell::Init()).
  EXPECT_EQ(ui::kCursorPointer, root_window->last_cursor().native_type());

  {
    // Resize edges and corners show proper cursors.
    window_delegate.set_hittest_code(HTBOTTOM);
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, point1, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move1);
    EXPECT_EQ(ui::kCursorSouthResize, root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTBOTTOMLEFT);
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED, point2, point2, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move2);
    EXPECT_EQ(ui::kCursorSouthWestResize,
              root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTBOTTOMRIGHT);
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, point1, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move1);
    EXPECT_EQ(ui::kCursorSouthEastResize,
              root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTLEFT);
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED, point2, point2, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move2);
    EXPECT_EQ(ui::kCursorWestResize, root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTRIGHT);
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, point1, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move1);
    EXPECT_EQ(ui::kCursorEastResize, root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTTOP);
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED, point2, point2, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move2);
    EXPECT_EQ(ui::kCursorNorthResize, root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTTOPLEFT);
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, point1, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move1);
    EXPECT_EQ(ui::kCursorNorthWestResize,
              root_window->last_cursor().native_type());
  }

  {
    window_delegate.set_hittest_code(HTTOPRIGHT);
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED, point2, point2, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move2);
    EXPECT_EQ(ui::kCursorNorthEastResize,
              root_window->last_cursor().native_type());
  }

  {
    // Client area uses null cursor.
    window_delegate.set_hittest_code(HTCLIENT);
    ui::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, point1, 0x0);
    root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&move1);
    EXPECT_EQ(ui::kCursorNull, root_window->last_cursor().native_type());
  }
}

#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_TransformActivate DISABLED_TransformActivate
#else
#define MAYBE_TransformActivate TransformActivate
#endif
TEST_F(WindowManagerTest, MAYBE_TransformActivate) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  gfx::Size size = root_window->bounds().size();
  EXPECT_EQ(gfx::Rect(size).ToString(),
            Shell::GetScreen()->GetDisplayNearestPoint(
                gfx::Point()).bounds().ToString());

  // Rotate it clock-wise 90 degrees.
  gfx::Transform transform;
  transform.Translate(size.width(), 0);
  transform.Rotate(90.0f);
  root_window->SetTransform(transform);

  test::TestActivationDelegate d1;
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> w1(
      CreateTestWindowInShellWithDelegate(&wd, 1, gfx::Rect(0, 10, 50, 50)));
  d1.SetWindow(w1.get());
  w1->Show();

  gfx::Point miss_point(5, 5);
  transform.TransformPoint(miss_point);
  ui::MouseEvent mouseev1(ui::ET_MOUSE_PRESSED,
                          miss_point,
                          miss_point,
                          ui::EF_LEFT_MOUSE_BUTTON);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouseev1);
  EXPECT_EQ(NULL, aura::client::GetFocusClient(w1.get())->GetFocusedWindow());
  ui::MouseEvent mouseup(ui::ET_MOUSE_RELEASED,
                         miss_point,
                         miss_point,
                         ui::EF_LEFT_MOUSE_BUTTON);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouseup);

  gfx::Point hit_point(5, 15);
  transform.TransformPoint(hit_point);
  ui::MouseEvent mouseev2(ui::ET_MOUSE_PRESSED,
                          hit_point,
                          hit_point,
                          ui::EF_LEFT_MOUSE_BUTTON);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouseev2);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(w1.get(),
            aura::client::GetFocusClient(w1.get())->GetFocusedWindow());
}

TEST_F(WindowManagerTest, AdditionalFilters) {
  // The IME event filter interferes with the basic key event propagation we
  // attempt to do here, so we remove it.
  test::ShellTestApi shell_test(Shell::GetInstance());
  Shell::GetInstance()->RemovePreTargetHandler(
      shell_test.input_method_event_filter());

  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  // Creates a window and make it active
  scoped_ptr<aura::Window> w1(CreateTestWindowInShell(
      SK_ColorWHITE, -1, gfx::Rect(0, 0, 100, 100)));
  wm::ActivateWindow(w1.get());

  // Creates two addition filters
  scoped_ptr<CustomEventHandler> f1(new CustomEventHandler);
  scoped_ptr<CustomEventHandler> f2(new CustomEventHandler);

  // Adds them to root window event filter.
  views::corewm::CompoundEventFilter* env_filter =
      Shell::GetInstance()->env_filter();
  env_filter->AddHandler(f1.get());
  env_filter->AddHandler(f2.get());

  // Dispatches mouse and keyboard events.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&key_event);
  ui::MouseEvent mouse_pressed(
      ui::ET_MOUSE_PRESSED, gfx::Point(0, 0), gfx::Point(0, 0), 0x0);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse_pressed);

  // Both filters should get the events.
  EXPECT_EQ(1, f1->num_key_events());
  EXPECT_EQ(1, f1->num_mouse_events());
  EXPECT_EQ(1, f2->num_key_events());
  EXPECT_EQ(1, f2->num_mouse_events());

  f1->Reset();
  f2->Reset();

  // Makes f1 consume events.
  f1->set_key_event_handling_result(ui::ER_CONSUMED);
  f1->set_mouse_event_handling_result(ui::ER_CONSUMED);

  // Dispatches events.
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&key_event);
  ui::MouseEvent mouse_released(
      ui::ET_MOUSE_RELEASED, gfx::Point(0, 0), gfx::Point(0, 0), 0x0);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse_released);

  // f1 should still get the events but f2 no longer gets them.
  EXPECT_EQ(1, f1->num_key_events());
  EXPECT_EQ(1, f1->num_mouse_events());
  EXPECT_EQ(0, f2->num_key_events());
  EXPECT_EQ(0, f2->num_mouse_events());

  f1->Reset();
  f2->Reset();

  // Remove f1 from additonal filters list.
  env_filter->RemoveHandler(f1.get());

  // Dispatches events.
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&key_event);
  root_window->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse_pressed);

  // f1 should get no events since it's out and f2 should get them.
  EXPECT_EQ(0, f1->num_key_events());
  EXPECT_EQ(0, f1->num_mouse_events());
  EXPECT_EQ(1, f2->num_key_events());
  EXPECT_EQ(1, f2->num_mouse_events());

  env_filter->RemoveHandler(f2.get());
}

// We should show and hide the cursor in response to mouse and touch events as
// requested.
TEST_F(WindowManagerTest, UpdateCursorVisibility) {
  aura::test::EventGenerator& generator = GetEventGenerator();
  views::corewm::CursorManager* cursor_manager =
      ash::Shell::GetInstance()->cursor_manager();

  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.PressTouch();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.ReleaseTouch();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // If someone else made cursor invisible keep it invisible even after it
  // received mouse events.
  cursor_manager->EnableMouseEvents();
  cursor_manager->HideCursor();
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.PressTouch();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.ReleaseTouch();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // Back to normal.
  cursor_manager->EnableMouseEvents();
  cursor_manager->ShowCursor();
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.PressTouch();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsMouseEventsEnabled());
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  generator.ReleaseTouch();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
}

TEST_F(WindowManagerTest, UpdateCursorVisibilityOnKeyEvent) {
  aura::test::EventGenerator& generator = GetEventGenerator();
  views::corewm::CursorManager* cursor_manager =
      ash::Shell::GetInstance()->cursor_manager();

  // Pressing a key hides the cursor but does not disable mouse events.
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Moving mouse shows the cursor.
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Releasing a key also hides the cursor but does not disable mouse events.
  generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
  // Moving mouse shows the cursor again.
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());
}

}  // namespace ash
