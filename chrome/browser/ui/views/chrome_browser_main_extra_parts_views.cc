// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/browser/ui/views/ime_driver_mus.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_AURA)
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/ui/public/cpp/gpu/gpu_service.h"
#include "services/ui/public/cpp/input_devices/input_device_client.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/input_devices/input_device_server.mojom.h"
#include "ui/display/screen.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif  // defined(USE_AURA)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

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
#if defined(USE_AURA) && !defined(OS_CHROMEOS) && !defined(USE_OZONE)
  // The screen may have already been set in test initialization.
  if (!display::Screen::GetScreen())
    display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif
}

void ChromeBrowserMainExtraPartsViews::PreProfileInit() {
#if defined(USE_AURA)
  // IME driver must be available at login screen, so initialize before profile.
  if (service_manager::ServiceManagerIsRemote())
    IMEDriver::Register();
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // On the Linux desktop, we want to prevent the user from logging in as root,
  // so that we don't destroy the profile. Now that we have some minimal ui
  // initialized, check to see if we're running as root and bail if we are.
  if (getuid() != 0)
    return;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUserDataDir))
    return;

  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_REFUSE_TO_RUN_AS_ROOT, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_REFUSE_TO_RUN_AS_ROOT_2, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  chrome::ShowWarningMessageBox(NULL, title, message);

  // Avoids gpu_process_transport_factory.cc(153)] Check failed:
  // per_compositor_data_.empty() when quit is chosen.
  base::RunLoop().RunUntilIdle();

  exit(EXIT_FAILURE);
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
}

void ChromeBrowserMainExtraPartsViews::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  DCHECK(connection);
#if defined(USE_AURA)
  if (service_manager::ServiceManagerIsRemote()) {
    // TODO(rockot): Remove the blocking wait for init.
    // http://crbug.com/594852.
    base::RunLoop wait_loop;
    connection->SetInitializeHandler(wait_loop.QuitClosure());
    wait_loop.Run();

    input_device_client_.reset(new ui::InputDeviceClient());
    ui::mojom::InputDeviceServerPtr server;
    connection->GetConnector()->ConnectToInterface(ui::mojom::kServiceName,
                                                   &server);
    input_device_client_->Connect(std::move(server));

    window_manager_connection_ = views::WindowManagerConnection::Create(
        connection->GetConnector(), connection->GetIdentity(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
  }
#endif  // defined(USE_AURA)
}
