// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/ime_driver/ime_driver_mus.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_AURA)
#include "base/run_loop.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/ui_devtools_css_agent.h"
#include "components/ui_devtools/views/ui_devtools_dom_agent.h"
#include "components/ui_devtools/views/ui_devtools_overlay_agent.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/ui/public/cpp/gpu/gpu.h"  // nogncheck
#include "services/ui/public/cpp/input_devices/input_device_client.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/input_devices/input_device_server.mojom.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"
#endif  // defined(USE_AURA)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ash_config.h"
#include "mash/common/config.h"                                   // nogncheck
#include "mash/quick_launch/public/interfaces/constants.mojom.h"  // nogncheck
#endif

namespace {

#if defined(USE_AURA)
bool ShouldCreateWMState() {
#if defined(OS_CHROMEOS)
  return chromeos::GetAshConfig() != ash::Config::MUS;
#else
  return true;
#endif
}
#endif

}  // namespace

ChromeBrowserMainExtraPartsViews::ChromeBrowserMainExtraPartsViews() {
}

ChromeBrowserMainExtraPartsViews::~ChromeBrowserMainExtraPartsViews() {
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
}

void ChromeBrowserMainExtraPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_ = base::MakeUnique<ChromeViewsDelegate>();

  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

#if defined(USE_AURA)
  if (ShouldCreateWMState())
    wm_state_.reset(new wm::WMState);
#endif
}

void ChromeBrowserMainExtraPartsViews::PreCreateThreads() {
#if defined(USE_AURA) && !defined(OS_CHROMEOS) && !defined(USE_OZONE)
  // The screen may have already been set in test initialization.
  if (!display::Screen::GetScreen())
    display::Screen::SetScreenInstance(views::CreateDesktopScreen());
#endif

  // TODO(pkasting): Try to move ViewsDelegate creation here as well;
  // see https://crbug.com/691894#c1
  // The layout_provider_ must be intialized here instead of in
  // ToolkitInitialized() because it relies on
  // ui::MaterialDesignController::Intialize() having already been called.
  if (!views::LayoutProvider::Get())
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
}

void ChromeBrowserMainExtraPartsViews::PreProfileInit() {
#if defined(USE_AURA)
  // IME driver must be available at login screen, so initialize before profile.
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS)
    IMEDriver::Register();

  // Start devtools server
  devtools_server_ = ui_devtools::UiDevToolsServer::Create(nullptr);
  if (devtools_server_) {
    auto dom_backend = base::MakeUnique<ui_devtools::UIDevToolsDOMAgent>();
    auto overlay_backend =
        base::MakeUnique<ui_devtools::UIDevToolsOverlayAgent>(
            dom_backend.get());
    auto css_backend =
        base::MakeUnique<ui_devtools::UIDevToolsCSSAgent>(dom_backend.get());
    auto devtools_client = base::MakeUnique<ui_devtools::UiDevToolsClient>(
        "UiDevToolsClient", devtools_server_.get());
    devtools_client->AddAgent(std::move(dom_backend));
    devtools_client->AddAgent(std::move(css_backend));
    devtools_client->AddAgent(std::move(overlay_backend));
    devtools_server_->AttachClient(std::move(devtools_client));
  }
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // On the Linux desktop, we want to prevent the user from logging in as root,
  // so that we don't destroy the profile. Now that we have some minimal ui
  // initialized, check to see if we're running as root and bail if we are.
  if (geteuid() != 0)
    return;

  // Allow running inside an unprivileged user namespace. In that case, the
  // root directory will be owned by an unmapped UID and GID (although this
  // may not be the case if a chroot is also being used).
  struct stat st;
  if (stat("/", &st) == 0 && st.st_uid != 0)
    return;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoSandbox))
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
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL)
    return;

#if defined(OS_CHROMEOS)
  if (chromeos::GetAshConfig() == ash::Config::MASH) {
    connection->GetConnector()->StartService(
        service_manager::Identity(ui::mojom::kServiceName));
    connection->GetConnector()->StartService(
        service_manager::Identity(mash::common::GetWindowManagerServiceName()));
    connection->GetConnector()->StartService(
        service_manager::Identity(mash::quick_launch::mojom::kServiceName));
  }
#endif

  input_device_client_ = base::MakeUnique<ui::InputDeviceClient>();
  ui::mojom::InputDeviceServerPtr server;
  connection->GetConnector()->BindInterface(ui::mojom::kServiceName, &server);
  input_device_client_->Connect(std::move(server));

#if defined(OS_CHROMEOS)
  if (chromeos::GetAshConfig() != ash::Config::MASH)
    return;
#endif

  // WMState is owned as a member, so don't have MusClient create it.
  const bool create_wm_state = false;
  mus_client_ = base::MakeUnique<views::MusClient>(
      connection->GetConnector(), service_manager::Identity(),
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO),
      create_wm_state);
#endif  // defined(USE_AURA)
}
