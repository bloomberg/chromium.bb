// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_launcher.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/public/app_registry.h"
#include "athena/content/public/content_activity_factory.h"
#include "athena/content/public/content_app_model_builder.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/home/public/home_card.h"
#include "athena/home/public/home_card.h"
#include "athena/input/public/input_manager.h"
#include "athena/main/debug/debug_window.h"
#include "athena/main/placeholder.h"
#include "athena/main/placeholder.h"
#include "athena/main/url_search_provider.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/system/public/system_ui.h"
#include "athena/virtual_keyboard/public/virtual_keyboard_manager.h"
#include "athena/wm/public/window_manager.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/aura/window_property.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/core/visibility_controller.h"

#if defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif

namespace athena {
struct AthenaEnvState;
}

DECLARE_WINDOW_PROPERTY_TYPE(athena::AthenaEnvState*);

namespace athena {

class VirtualKeyboardObserver;

// Athena's env state.
struct AthenaEnvState {
  scoped_ptr< ::wm::VisibilityController> visibility_client;
  scoped_ptr<VirtualKeyboardObserver> virtual_keyboard_observer;
};

DEFINE_OWNED_WINDOW_PROPERTY_KEY(athena::AthenaEnvState,
                                 kAthenaEnvStateKey,
                                 NULL);

// This class observes the change of the virtual keyboard and distribute the
// change to appropriate modules of athena.
// TODO(oshima): move the VK bounds logic to screen manager.
class VirtualKeyboardObserver : public keyboard::KeyboardControllerObserver {
 public:
  VirtualKeyboardObserver() {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
  }

  virtual ~VirtualKeyboardObserver() {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
  }

 private:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE {
    HomeCard::Get()->UpdateVirtualKeyboardBounds(new_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardObserver);
};

class AthenaViewsDelegate : public views::ViewsDelegate {
 public:
  AthenaViewsDelegate() {}
  virtual ~AthenaViewsDelegate() {}

 private:
  // views::ViewsDelegate:
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaViewsDelegate);
};

void StartAthenaEnv(aura::Window* root_window,
                    athena::ScreenManagerDelegate* delegate,
                    scoped_refptr<base::TaskRunner> file_runner) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Force showing in the experimental app-list view.
  command_line->AppendSwitch(app_list::switches::kEnableExperimentalAppList);
  command_line->AppendSwitch(switches::kEnableOverlayScrollbar);

#if defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

  views::ViewsDelegate::views_delegate = new AthenaViewsDelegate();

  AthenaEnvState* env_state = new AthenaEnvState;

  // Setup VisibilityClient
  env_state->visibility_client.reset(new ::wm::VisibilityController);

  aura::client::SetVisibilityClient(root_window,
                                    env_state->visibility_client.get());

  athena::SystemUI::Create(file_runner);
  athena::InputManager::Create()->OnRootWindowCreated(root_window);
  athena::ScreenManager::Create(delegate, root_window);
  athena::WindowManager::Create();
  athena::AppRegistry::Create();
  SetupBackgroundImage();

  athena::ScreenManager::Get()->GetContext()->SetProperty(
      kAthenaEnvStateKey, env_state);
}

void StartAthenaSessionWithContext(content::BrowserContext* context) {
  athena::ExtensionsDelegate::CreateExtensionsDelegateForShell(context);
  StartAthenaSession(new athena::ContentActivityFactory(),
                     new athena::ContentAppModelBuilder(context));
  athena::VirtualKeyboardManager::Create(context);
  athena::HomeCard::Get()->RegisterSearchProvider(
      new athena::UrlSearchProvider(context));
  AthenaEnvState* env_state =
      athena::ScreenManager::Get()->GetContext()->GetProperty(
          kAthenaEnvStateKey);

  env_state->virtual_keyboard_observer.reset(new VirtualKeyboardObserver);
  CreateTestPages(context);
  CreateDebugWindow();
}

void StartAthenaSession(athena::ActivityFactory* activity_factory,
                        athena::AppModelBuilder* app_model_builder) {
  athena::HomeCard::Create(app_model_builder);
  athena::ActivityManager::Create();
  athena::ActivityFactory::RegisterActivityFactory(activity_factory);
}

void ShutdownAthena() {
  athena::ActivityFactory::Shutdown();
  athena::ActivityManager::Shutdown();
  athena::HomeCard::Shutdown();
  athena::AppRegistry::ShutDown();
  athena::WindowManager::Shutdown();
  athena::ScreenManager::Shutdown();
  athena::InputManager::Shutdown();
  athena::SystemUI::Shutdown();
  athena::ExtensionsDelegate::Shutdown();

  delete views::ViewsDelegate::views_delegate;
}

}  // namespace athena
