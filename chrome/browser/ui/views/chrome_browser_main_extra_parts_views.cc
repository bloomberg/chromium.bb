// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_AURA)
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/runner/common/client_util.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/input_devices/input_device_client.h"
#include "services/ui/public/interfaces/input_devices/input_device_server.mojom.h"
#include "ui/display/screen.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif  // defined(USE_AURA)

ChromeBrowserMainExtraPartsViews::ChromeBrowserMainExtraPartsViews() {
}

ChromeBrowserMainExtraPartsViews::~ChromeBrowserMainExtraPartsViews() {
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}

void ChromeBrowserMainExtraPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_.reset(new ChromeViewsDelegate);

  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

#if defined(USE_AURA)
  wm_state_.reset(new wm::WMState);
#endif
}

void ChromeBrowserMainExtraPartsViews::PreCreateThreads() {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // The screen may have already been set in test initialization.
  if (!display::Screen::GetScreen())
    display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif
}

void ChromeBrowserMainExtraPartsViews::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  DCHECK(connection);
#if defined(USE_AURA)
  if (shell::ShellIsRemote()) {
    // TODO(rockot): Remove the blocking wait for init.
    // http://crbug.com/594852.
    base::RunLoop wait_loop;
    connection->SetInitializeHandler(wait_loop.QuitClosure());
    wait_loop.Run();

    input_device_client_.reset(new ui::InputDeviceClient());
    ui::mojom::InputDeviceServerPtr server;
    connection->GetConnector()->ConnectToInterface("service:ui", &server);
    input_device_client_->Connect(std::move(server));

    window_manager_connection_ = views::WindowManagerConnection::Create(
        connection->GetConnector(), connection->GetIdentity(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
  }
#endif  // defined(USE_AURA)
}
