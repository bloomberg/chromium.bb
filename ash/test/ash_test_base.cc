// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/mus/top_level_window_factory.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_positioner.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/core/coordinate_conversion.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#endif

namespace ash {
namespace test {
namespace {

class AshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  AshEventGeneratorDelegate() {}
  ~AshEventGeneratorDelegate() override {}

  // aura::test::EventGeneratorDelegateAura overrides:
  aura::WindowTreeHost* GetHostAt(
      const gfx::Point& point_in_screen) const override {
    display::Screen* screen = display::Screen::GetScreen();
    display::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    return Shell::GetRootWindowForDisplayId(display.id())->GetHost();
  }

  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return aura::client::GetScreenPositionClient(window->GetRootWindow());
  }

  void DispatchKeyEventToIME(ui::EventTarget* target,
                             ui::KeyEvent* event) override {
    // In Ash environment, the key event will be processed by event rewriters
    // first.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshEventGeneratorDelegate);
};

ui::mojom::WindowType MusWindowTypeFromWindowType(
    aura::client::WindowType window_type) {
  switch (window_type) {
    case aura::client::WINDOW_TYPE_UNKNOWN:
      break;

    case aura::client::WINDOW_TYPE_NORMAL:
      return ui::mojom::WindowType::WINDOW;

    case aura::client::WINDOW_TYPE_POPUP:
      return ui::mojom::WindowType::POPUP;

    case aura::client::WINDOW_TYPE_CONTROL:
      return ui::mojom::WindowType::CONTROL;

    case aura::client::WINDOW_TYPE_PANEL:
      return ui::mojom::WindowType::PANEL;

    case aura::client::WINDOW_TYPE_MENU:
      return ui::mojom::WindowType::MENU;

    case aura::client::WINDOW_TYPE_TOOLTIP:
      return ui::mojom::WindowType::TOOLTIP;
  }

  NOTREACHED();
  return ui::mojom::WindowType::CONTROL;
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////

AshTestBase::AshTestBase()
    : setup_called_(false), teardown_called_(false), start_session_(true) {
#if defined(USE_X11)
  // This is needed for tests which use this base class but are run in browser
  // test binaries so don't get the default initialization in the unit test
  // suite.
  gfx::InitializeThreadedX11();
#endif

  ash_test_environment_ = AshTestEnvironment::Create();

  // Must initialize |ash_test_helper_| here because some tests rely on
  // AshTestBase methods before they call AshTestBase::SetUp().
  ash_test_helper_.reset(new AshTestHelper(ash_test_environment_.get()));
}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::UnblockCompositors() {
  // In order for frames to be generated, a cc::LocalSurfaceId must be given to
  // the ui::Compositor. Normally that cc::LocalSurfaceId comes from the window
  // server but in unit tests, there is no window server so we just make up a
  // cc::LocalSurfaceId to allow the layer compositor to make forward progress.
  if (Shell::GetAshConfig() == Config::MUS ||
      Shell::GetAshConfig() == Config::MASH) {
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    for (aura::Window* root : root_windows) {
      cc::LocalSurfaceId id(1, base::UnguessableToken::Create());
      root->GetHost()->compositor()->SetLocalSurfaceId(id);
    }
  }
}

void AshTestBase::SetUp() {
  setup_called_ = true;

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();

  ash_test_helper_->SetUp(start_session_);

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  // TODO: mus/mash needs to support CursorManager. http://crbug.com/637853.
  if (Shell::GetAshConfig() == Config::CLASSIC)
    Shell::Get()->cursor_manager()->EnableMouseEvents();

  UnblockCompositors();

  // Changing GestureConfiguration shouldn't make tests fail. These values
  // prevent unexpected events from being generated during tests. Such as
  // delayed events which create race conditions on slower tests.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);
}

void AshTestBase::TearDown() {
  teardown_called_ = true;
  Shell::Get()->session_controller()->NotifyChromeTerminating();

  // Some tasks are blocked on progress by the layer compositor. The layer
  // compositor might be blocked if created during a unit test.
  UnblockCompositors();

  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  ash_test_helper_->TearDown();

  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  display::Display::SetInternalDisplayId(display::kInvalidDisplayId);
}

// static
Shelf* AshTestBase::GetPrimaryShelf() {
  return Shell::GetPrimaryRootWindowController()->shelf();
}

// static
SystemTray* AshTestBase::GetPrimarySystemTray() {
  return Shell::Get()->GetPrimarySystemTray();
}

ui::test::EventGenerator& AshTestBase::GetEventGenerator() {
  if (!event_generator_) {
    event_generator_.reset(
        new ui::test::EventGenerator(new AshEventGeneratorDelegate()));
  }
  return *event_generator_.get();
}

// static
display::Display::Rotation AshTestBase::GetActiveDisplayRotation(int64_t id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(id)
      .GetActiveRotation();
}

// static
display::Display::Rotation AshTestBase::GetCurrentInternalDisplayRotation() {
  return GetActiveDisplayRotation(display::Display::InternalDisplayId());
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay(display_specs);
  UnblockCompositors();
}

aura::Window* AshTestBase::CurrentContext() {
  return ash_test_helper_->CurrentContext();
}

// static
std::unique_ptr<views::Widget> AshTestBase::CreateTestWidget(
    views::WidgetDelegate* delegate,
    int container_id,
    const gfx::Rect& bounds) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  Shell::GetPrimaryRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(widget.get(), container_id,
                                              &params);
  widget->Init(params);
  widget->Show();
  return widget;
}

std::unique_ptr<aura::Window> AshTestBase::CreateTestWindow(
    const gfx::Rect& bounds_in_screen,
    aura::client::WindowType type,
    int shell_window_id) {
  if (AshTestHelper::config() != Config::MASH) {
    return base::WrapUnique<aura::Window>(
        CreateTestWindowInShellWithDelegateAndType(
            nullptr, type, shell_window_id, bounds_in_screen));
  }

  // For mash route creation through the window manager. This better simulates
  // what happens when a client creates a top level window.
  std::map<std::string, std::vector<uint8_t>> properties;
  if (!bounds_in_screen.IsEmpty()) {
    properties[ui::mojom::WindowManager::kBounds_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(bounds_in_screen);
  }

  properties[ui::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<aura::PropertyConverter::PrimitiveType>(
              ui::mojom::kResizeBehaviorCanResize |
              ui::mojom::kResizeBehaviorCanMaximize |
              ui::mojom::kResizeBehaviorCanMinimize));

  const ui::mojom::WindowType mus_window_type =
      MusWindowTypeFromWindowType(type);
  mus::WindowManager* window_manager =
      ash_test_helper_->window_manager_app()->window_manager();
  aura::Window* window = mus::CreateAndParentTopLevelWindow(
      window_manager, mus_window_type, &properties);
  window->set_id(shell_window_id);
  window->Show();
  return base::WrapUnique<aura::Window>(window);
}

std::unique_ptr<aura::Window> AshTestBase::CreateToplevelTestWindow(
    const gfx::Rect& bounds_in_screen,
    int shell_window_id) {
  if (AshTestHelper::config() == Config::MASH) {
    return CreateTestWindow(bounds_in_screen, aura::client::WINDOW_TYPE_NORMAL,
                            shell_window_id);
  }

  aura::test::TestWindowDelegate* delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  return base::WrapUnique<aura::Window>(
      CreateTestWindowInShellWithDelegateAndType(
          delegate, aura::client::WINDOW_TYPE_NORMAL, shell_window_id,
          bounds_in_screen));
}

aura::Window* AshTestBase::CreateTestWindowInShellWithId(int id) {
  return CreateTestWindowInShellWithDelegate(NULL, id, gfx::Rect());
}

aura::Window* AshTestBase::CreateTestWindowInShellWithBounds(
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(NULL, 0, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShell(SkColor color,
                                                   int id,
                                                   const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(
      new aura::test::ColorTestWindowDelegate(color), id, bounds);
}

std::unique_ptr<aura::Window> AshTestBase::CreateChildWindow(
    aura::Window* parent,
    const gfx::Rect& bounds,
    int shell_window_id) {
  std::unique_ptr<aura::Window> window =
      base::MakeUnique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  window->set_id(shell_window_id);
  parent->AddChild(window.get());
  window->Show();
  return window;
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate, aura::client::WINDOW_TYPE_NORMAL, id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegateAndType(
    aura::WindowDelegate* delegate,
    aura::client::WindowType type,
    int id,
    const gfx::Rect& bounds) {
  aura::Window* window = new aura::Window(delegate);
  window->set_id(id);
  window->SetType(type);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();

  if (bounds.IsEmpty()) {
    ParentWindowInPrimaryRootWindow(window);
  } else {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    aura::Window* root = Shell::GetRootWindowForDisplayId(display.id());
    gfx::Point origin = bounds.origin();
    ::wm::ConvertPointFromScreen(root, &origin);
    window->SetBounds(gfx::Rect(origin, bounds.size()));
    aura::client::ParentWindowWithContext(window, root, bounds);
  }
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      ui::mojom::kResizeBehaviorCanMaximize |
                          ui::mojom::kResizeBehaviorCanMinimize |
                          ui::mojom::kResizeBehaviorCanResize);
  // Setting the item type triggers ShelfWindowWatcher to create a shelf item.
  if (type == aura::client::WINDOW_TYPE_PANEL)
    window->SetProperty<int>(kShelfItemTypeKey, TYPE_APP_PANEL);

  return window;
}

void AshTestBase::ParentWindowInPrimaryRootWindow(aura::Window* window) {
  aura::client::ParentWindowWithContext(window, Shell::GetPrimaryRootWindow(),
                                        gfx::Rect());
}

void AshTestBase::RunAllPendingInMessageLoop() {
  ash_test_helper_->RunAllPendingInMessageLoop();
}

TestScreenshotDelegate* AshTestBase::GetScreenshotDelegate() {
  return ash_test_helper_->test_screenshot_delegate();
}

TestSessionControllerClient* AshTestBase::GetSessionControllerClient() {
  return ash_test_helper_->test_session_controller_client();
}

void AshTestBase::SetSessionStarted(bool session_started) {
  if (session_started)
    GetSessionControllerClient()->CreatePredefinedUserSessions(1);
  else
    GetSessionControllerClient()->Reset();
}

void AshTestBase::SetUserLoggedIn(bool user_logged_in) {
  SetSessionStarted(user_logged_in);
}

void AshTestBase::SetCanLockScreen(bool can_lock) {
  GetSessionControllerClient()->SetCanLockScreen(can_lock);
}

void AshTestBase::SetShouldLockScreenAutomatically(bool should_lock) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(should_lock);
}

void AshTestBase::SetUserAddingScreenRunning(bool user_adding_screen_running) {
  GetSessionControllerClient()->SetSessionState(
      user_adding_screen_running
          ? session_manager::SessionState::LOGIN_SECONDARY
          : session_manager::SessionState::ACTIVE);
}

void AshTestBase::BlockUserSession(UserSessionBlockReason block_reason) {
  switch (block_reason) {
    case BLOCKED_BY_LOCK_SCREEN:
      SetSessionStarted(true);
      Shell::Get()->session_controller()->LockScreenAndFlushForTest();
      break;
    case BLOCKED_BY_LOGIN_SCREEN:
      SetSessionStarted(false);
      break;
    case BLOCKED_BY_USER_ADDING_SCREEN:
      SetUserAddingScreenRunning(true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AshTestBase::UnblockUserSession() {
  SetSessionStarted(true);
  GetSessionControllerClient()->UnlockScreen();
}

void AshTestBase::DisableIME() {
  aura::test::DisableIME(Shell::GetPrimaryRootWindow()->GetHost());
}

display::DisplayManager* AshTestBase::display_manager() {
  return Shell::Get()->display_manager();
}

bool AshTestBase::TestIfMouseWarpsAt(ui::test::EventGenerator& event_generator,
                                     const gfx::Point& point_in_screen) {
  DCHECK(!Shell::Get()->display_manager()->IsInUnifiedMode());
  static_cast<ExtendedMouseWarpController*>(
      Shell::Get()->mouse_cursor_filter()->mouse_warp_controller_for_test())
      ->allow_non_native_event_for_test();
  display::Screen* screen = display::Screen::GetScreen();
  display::Display original_display =
      screen->GetDisplayNearestPoint(point_in_screen);
  event_generator.MoveMouseTo(point_in_screen);
  return original_display.id() !=
         screen
             ->GetDisplayNearestPoint(
                 aura::Env::GetInstance()->last_mouse_location())
             .id();
}

void AshTestBase::SwapPrimaryDisplay() {
  if (display::Screen::GetScreen()->GetNumDisplays() <= 1)
    return;
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetSecondaryDisplay().id());
}

display::Display AshTestBase::GetPrimaryDisplay() {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
}

display::Display AshTestBase::GetSecondaryDisplay() {
  return ash_test_helper_->GetSecondaryDisplay();
}

}  // namespace test
}  // namespace ash
