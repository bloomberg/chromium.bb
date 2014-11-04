// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/public/athena_launcher.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/public/app_registry.h"
#include "athena/content/public/content_activity_factory_creator.h"
#include "athena/env/public/athena_env.h"
#include "athena/extensions/public/apps_search_controller_factory.h"
#include "athena/extensions/public/extension_app_model_builder.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/home/public/home_card.h"
#include "athena/home/public/search_controller_factory.h"
#include "athena/input/public/input_manager.h"
#include "athena/main/athena_views_delegate.h"
#include "athena/main/debug_accelerator_handler.h"
#include "athena/main/placeholder.h"
#include "athena/main/placeholder.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/screen_lock/public/screen_lock_manager.h"
#include "athena/system/public/system_ui.h"
#include "athena/virtual_keyboard/public/virtual_keyboard_manager.h"
#include "athena/wm/public/window_manager.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/content_switches.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/wm/core/visibility_controller.h"

#if defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"
#endif

namespace athena {
struct AthenaEnvState;
}

DECLARE_WINDOW_PROPERTY_TYPE(athena::AthenaEnvState*);

namespace athena {

namespace {

bool session_started = false;

}  // namespace

class VirtualKeyboardObserver;

// Athena's env state.
struct AthenaEnvState {
  scoped_ptr< ::wm::VisibilityController> visibility_client;
  scoped_ptr<VirtualKeyboardObserver> virtual_keyboard_observer;
  scoped_ptr<DebugAcceleratorHandler> debug_accelerator_handler;
};

DEFINE_OWNED_WINDOW_PROPERTY_KEY(athena::AthenaEnvState,
                                 kAthenaEnvStateKey,
                                 nullptr);

// This class observes the change of the virtual keyboard and distribute the
// change to appropriate modules of athena.
// TODO(oshima): move the VK bounds logic to screen manager.
class VirtualKeyboardObserver : public keyboard::KeyboardControllerObserver {
 public:
  VirtualKeyboardObserver() {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
  }

  ~VirtualKeyboardObserver() override {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
  }

 private:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override {
    HomeCard::Get()->UpdateVirtualKeyboardBounds(new_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardObserver);
};

void StartAthenaEnv(scoped_refptr<base::TaskRunner> blocking_task_runner) {
  athena::AthenaEnv::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Force showing in the experimental app-list view.
  command_line->AppendSwitch(app_list::switches::kEnableExperimentalAppList);
  command_line->AppendSwitch(switches::kEnableOverlayScrollbar);

  // Enable vertical overscroll.
  command_line->AppendSwitchASCII(switches::kScrollEndEffect, "1");

#if defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

  CreateAthenaViewsDelegate();

  AthenaEnvState* env_state = new AthenaEnvState;

  // Setup VisibilityClient
  env_state->visibility_client.reset(new ::wm::VisibilityController);
  aura::Window* root_window = athena::AthenaEnv::Get()->GetHost()->window();

  aura::client::SetVisibilityClient(root_window,
                                    env_state->visibility_client.get());

  athena::InputManager::Create()->OnRootWindowCreated(root_window);
  athena::ScreenManager::Create(root_window);
  athena::WindowManager::Create();
  athena::SystemUI::Create(blocking_task_runner);
  athena::AppRegistry::Create();
  SetupBackgroundImage();

  env_state->debug_accelerator_handler.reset(
      new DebugAcceleratorHandler(root_window));

  athena::ScreenManager::Get()->GetContext()->SetProperty(
      kAthenaEnvStateKey, env_state);
}

void CreateVirtualKeyboardWithContext(content::BrowserContext* context) {
  athena::VirtualKeyboardManager::Create(context);
}

void StartAthenaSessionWithContext(content::BrowserContext* context) {
  athena::ScreenLockManager::Create();
  athena::ExtensionsDelegate::CreateExtensionsDelegate(context);
  StartAthenaSession(
      athena::CreateContentActivityFactory(),
      make_scoped_ptr(new athena::ExtensionAppModelBuilder(context)),
      athena::CreateSearchControllerFactory(context));
  AthenaEnvState* env_state =
      athena::ScreenManager::Get()->GetContext()->GetProperty(
          kAthenaEnvStateKey);

  env_state->virtual_keyboard_observer.reset(new VirtualKeyboardObserver);
  CreateTestPages(context);
}

void StartAthenaSession(
    athena::ActivityFactory* activity_factory,
    scoped_ptr<athena::AppModelBuilder> app_model_builder,
    scoped_ptr<athena::SearchControllerFactory> search_factory) {
  DCHECK(!session_started);
  session_started = true;
  athena::HomeCard::Create(app_model_builder.Pass(), search_factory.Pass());
  athena::ActivityManager::Create();
  athena::ResourceManager::Create();
  athena::ActivityFactory::RegisterActivityFactory(activity_factory);
}

void ShutdownAthena() {
  if (session_started) {
    athena::ActivityFactory::Shutdown();
    athena::ResourceManager::Shutdown();
    athena::ActivityManager::Shutdown();
    athena::HomeCard::Shutdown();
    athena::ExtensionsDelegate::Shutdown();
    athena::ScreenLockManager::Shutdown();
    session_started = false;
  }
  athena::AppRegistry::ShutDown();
  athena::SystemUI::Shutdown();
  athena::WindowManager::Shutdown();
  athena::ScreenManager::Shutdown();
  athena::InputManager::Shutdown();
  athena::AthenaEnv::Shutdown();

  ShutdownAthenaViewsDelegate();
}

}  // namespace athena
