// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_launcher.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/home/public/home_card.h"
#include "athena/input/public/input_manager.h"
#include "athena/main/placeholder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/system/public/system_ui.h"
#include "athena/wm/public/window_manager.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window_property.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/core/visibility_controller.h"

#if defined(USE_X11)
#include "ui/events/x/touch_factory_x11.h"
#endif

namespace athena {
struct RootWindowState;
}

DECLARE_WINDOW_PROPERTY_TYPE(athena::RootWindowState*);

namespace athena {

// Athena's per root window state.
struct RootWindowState {
  scoped_ptr< ::wm::VisibilityController> visibility_client;
};

DEFINE_OWNED_WINDOW_PROPERTY_KEY(athena::RootWindowState,
                                 kRootWindowStateKey,
                                 NULL);

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

void StartAthena(aura::Window* root_window,
                 athena::ActivityFactory* activity_factory,
                 athena::AppModelBuilder* app_model_builder) {
#if defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

  views::ViewsDelegate::views_delegate = new AthenaViewsDelegate();

  RootWindowState* root_window_state = new RootWindowState;
  root_window->SetProperty(kRootWindowStateKey, root_window_state);

  root_window_state->visibility_client.reset(new ::wm::VisibilityController);
  aura::client::SetVisibilityClient(root_window,
                                    root_window_state->visibility_client.get());

  athena::SystemUI::Create(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  athena::InputManager::Create()->OnRootWindowCreated(root_window);
  athena::ScreenManager::Create(root_window);
  athena::WindowManager::Create();
  athena::HomeCard::Create(app_model_builder);
  athena::ActivityManager::Create();
  athena::ActivityFactory::RegisterActivityFactory(activity_factory);
  SetupBackgroundImage();
}

void ShutdownAthena() {
  athena::ActivityFactory::Shutdown();
  athena::ActivityManager::Shutdown();
  athena::HomeCard::Shutdown();
  athena::WindowManager::Shutdown();
  athena::ScreenManager::Shutdown();
  athena::InputManager::Shutdown();
  athena::SystemUI::Shutdown();

  delete views::ViewsDelegate::views_delegate;
}

}  // namespace athena
