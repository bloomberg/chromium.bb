// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/material_design/material_design_controller.h"

#if defined(USE_AURA)
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "components/ui_devtools/views/overlay_agent_aura.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/ws/public/cpp/gpu/gpu.h"  // nogncheck
#include "services/ws/public/mojom/constants.mojom.h"
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

#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "ui/base/ui_base_features.h"
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
    views_delegate_ = std::make_unique<ChromeViewsDelegate>();

  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

  // The MaterialDesignController needs to look at command line flags, which
  // are not available until this point. Now that they are, proceed with
  // initializing the MaterialDesignController.
  ui::MaterialDesignController::Initialize();

#if defined(USE_AURA)
  wm_state_.reset(new wm::WMState);
#endif

  // TODO(pkasting): Try to move ViewsDelegate creation here as well;
  // see https://crbug.com/691894#c1
  if (!views::LayoutProvider::Get())
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
}

void ChromeBrowserMainExtraPartsViews::PreCreateThreads() {
#if defined(USE_AURA)
  views::InstallDesktopScreenIfNecessary();
#endif
}

void ChromeBrowserMainExtraPartsViews::PreProfileInit() {
#if defined(USE_AURA)
  // Start devtools server
  constexpr int kUiDevToolsDefaultPort = 9223;
  network::mojom::NetworkContext* network_context =
      g_browser_process->system_network_context_manager()->GetContext();
  devtools_server_ = ui_devtools::UiDevToolsServer::CreateForViews(
      network_context, switches::kEnableUiDevTools, kUiDevToolsDefaultPort);
  if (devtools_server_) {
    auto dom_backend = std::make_unique<ui_devtools::DOMAgentAura>();
#if defined(OS_CHROMEOS)
    // OverlayAgentAura intends to handle input events targeting any UI surface,
    // and so installs itself as a local aura::Env pre-target ui::EventHandler.
    // In multi-process Mash, Chrome's local aura::Env can only handle events
    // target Chrome's own aura::Windows, not those targeting Ash or mojo apps.
    // TODO(crbug.com/896977): Init the devtools server in Ash on Chrome OS.
    LOG_IF(WARNING, features::IsMultiProcessMash())
        << "Chrome cannot handle Ash system ui and mojo app events in Mash.";
#endif
    auto overlay_backend =
        std::make_unique<ui_devtools::OverlayAgentAura>(dom_backend.get());
    auto css_backend =
        std::make_unique<ui_devtools::CSSAgent>(dom_backend.get());
    auto devtools_client = std::make_unique<ui_devtools::UiDevToolsClient>(
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
  if (command_line.HasSwitch(service_manager::switches::kNoSandbox))
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

void ChromeBrowserMainExtraPartsViews::PostBrowserStart() {
  relaunch_notification_controller_ =
      std::make_unique<RelaunchNotificationController>(
          UpgradeDetector::GetInstance());
}

void ChromeBrowserMainExtraPartsViews::PostMainMessageLoopRun() {
  // The relaunch notification controller acts on timer-based events. Tear it
  // down explicitly here to avoid a case where such an event arrives during
  // shutdown.
  relaunch_notification_controller_.reset();
}
