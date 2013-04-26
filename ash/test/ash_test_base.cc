// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {
namespace {

class AshEventGeneratorDelegate : public aura::test::EventGeneratorDelegate {
 public:
  AshEventGeneratorDelegate() {}
  virtual ~AshEventGeneratorDelegate() {}

  // aura::test::EventGeneratorDelegate overrides:
  virtual aura::RootWindow* GetRootWindowAt(
      const gfx::Point& point_in_screen) const OVERRIDE {
    gfx::Screen* screen = Shell::GetScreen();
    gfx::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    return Shell::GetInstance()->display_controller()->
        GetRootWindowForDisplayId(display.id());
  }

  virtual aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const OVERRIDE {
    return aura::client::GetScreenPositionClient(window->GetRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshEventGeneratorDelegate);
};

}  // namespace

content::WebContents* AshTestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                           site_instance);
}

/////////////////////////////////////////////////////////////////////////////

AshTestBase::AshTestBase()
    : setup_called_(false),
      teardown_called_(false) {
  // Must initialize |ash_test_helper_| here because some tests rely on
  // AshTestBase methods before they call AshTestBase::SetUp().
  ash_test_helper_.reset(new AshTestHelper(&message_loop_));
}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  setup_called_ = true;
  ash_test_helper_->SetUp();
}

void AshTestBase::TearDown() {
  teardown_called_ = true;
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();
  ash_test_helper_->TearDown();
  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  gfx::Display::SetInternalDisplayId(gfx::Display::kInvalidDisplayID);
}

aura::test::EventGenerator& AshTestBase::GetEventGenerator() {
  if (!event_generator_) {
    event_generator_.reset(
        new aura::test::EventGenerator(new AshEventGeneratorDelegate()));
  }
  return *event_generator_.get();
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay(display_specs);
}

aura::RootWindow* AshTestBase::CurrentContext() {
  return ash_test_helper_->CurrentContext();
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

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate,
      aura::client::WINDOW_TYPE_NORMAL,
      id,
      bounds);
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
    SetDefaultParentByPrimaryRootWindow(window);
  } else {
    gfx::Display display =
      ash::Shell::GetInstance()->display_manager()->GetDisplayMatching(bounds);
    aura::RootWindow* root = ash::Shell::GetInstance()->display_controller()->
        GetRootWindowForDisplayId(display.id());
    gfx::Point origin = bounds.origin();
    wm::ConvertPointFromScreen(root, &origin);
    window->SetBounds(gfx::Rect(origin, bounds.size()));
    window->SetDefaultParentByRootWindow(root, bounds);
  }
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  return window;
}

void AshTestBase::SetDefaultParentByPrimaryRootWindow(aura::Window* window) {
  window->SetDefaultParentByRootWindow(
      Shell::GetPrimaryRootWindow(), gfx::Rect());
}

void AshTestBase::RunAllPendingInMessageLoop() {
  ash_test_helper_->RunAllPendingInMessageLoop();
}

void AshTestBase::SetSessionStarted(bool session_started) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetActiveUserSessionStarted(session_started);
}

void AshTestBase::SetUserLoggedIn(bool user_logged_in) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetHasActiveUser(user_logged_in);
}

void AshTestBase::SetCanLockScreen(bool can_lock_screen) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetCanLockScreen(can_lock_screen);
}

}  // namespace test
}  // namespace ash
