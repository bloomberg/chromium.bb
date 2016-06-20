// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_AURA)
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif

#if defined(USE_AURA) && defined(MOJO_SHELL_CLIENT)
#include "components/mus/public/cpp/input_devices/input_device_client.h"
#include "components/mus/public/interfaces/input_devices/input_device_server.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/runner/common/client_util.h"
#include "ui/views/mus/window_manager_connection.h"
#endif

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
  display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif
}

void ChromeBrowserMainExtraPartsViews::PreProfileInit() {
#if defined(USE_AURA) && defined(MOJO_SHELL_CLIENT)
  content::MojoShellConnection* mojo_shell_connection =
      content::MojoShellConnection::GetForProcess();
  if (mojo_shell_connection && shell::ShellIsRemote()) {
    input_device_client_.reset(new mus::InputDeviceClient());
    mus::mojom::InputDeviceServerPtr server;
    mojo_shell_connection->GetConnector()->ConnectToInterface("mojo:mus",
                                                              &server);
    input_device_client_->Connect(std::move(server));

    window_manager_connection_ = views::WindowManagerConnection::Create(
        mojo_shell_connection->GetConnector(),
        mojo_shell_connection->GetIdentity());
  }
#endif  // defined(USE_AURA) && defined(MOJO_SHELL_CLIENT)
  ChromeBrowserMainExtraParts::PreProfileInit();
}
