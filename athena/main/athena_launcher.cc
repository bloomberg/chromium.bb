// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_launcher.h"

#include "athena/home/public/home_card.h"
#include "athena/main/placeholder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_property.h"
#include "ui/wm/core/visibility_controller.h"

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

void StartAthena(aura::Window* root_window) {
  RootWindowState* root_window_state = new RootWindowState;
  root_window->SetProperty(kRootWindowStateKey, root_window_state);

  root_window_state->visibility_client.reset(new ::wm::VisibilityController);
  aura::client::SetVisibilityClient(root_window,
                                    root_window_state->visibility_client.get());

  athena::ScreenManager::Create(root_window);
  athena::WindowManager::Create();
  athena::HomeCard::Create();

  SetupBackgroundImage();
}

void ShutdownAthena() {
  athena::HomeCard::Shutdown();
  athena::WindowManager::Shutdown();
  athena::ScreenManager::Shutdown();
}

}  // namespace athena
