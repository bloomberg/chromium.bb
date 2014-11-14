// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_app_window_client_base.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "extensions/common/extension.h"
#include "extensions/components/native_app_window/native_app_window_views.h"

namespace athena {

AthenaAppWindowClientBase::AthenaAppWindowClientBase() {
}

AthenaAppWindowClientBase::~AthenaAppWindowClientBase() {
}

extensions::NativeAppWindow* AthenaAppWindowClientBase::CreateNativeAppWindow(
    extensions::AppWindow* app_window,
    extensions::AppWindow::CreateParams* params) {
  auto* native_window = new native_app_window::NativeAppWindowViews;
  native_window->Init(app_window, *params);
  ActivityFactory::Get()->CreateAppActivity(app_window->extension_id(),
                                            native_window->web_view());
  if (params->focused) {
    // Windows are created per default at the top of the stack. If - at this
    // point of initialization - it is has been moved into a different Z-order
    // location we should respect this, not allowing the application activation
    // to bring it to the front. This can happen as part of the resource
    // manager's reloading or intelligent preloading of an application.
    const aura::Window::Windows& list =
        WindowManager::Get()->GetWindowListProvider()->GetWindowList();
    aura::Window* native_app_window =
        native_window->widget()->GetNativeWindow();
    params->focused = !list.size() || list.back() == native_app_window;
  }
  return native_window;
}

}  // namespace athena
