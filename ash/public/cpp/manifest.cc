// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/manifest.h"

#include "ash/public/interfaces/accelerator_controller.mojom.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/public/interfaces/accessibility_focus_ring_controller.mojom.h"
#include "ash/public/interfaces/app_list.mojom.h"
#include "ash/public/interfaces/arc_custom_tab.mojom.h"
#include "ash/public/interfaces/ash_display_controller.mojom.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/assistant_volume_control.mojom.h"
#include "ash/public/interfaces/cast_config.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "ash/public/interfaces/display_output_protection.mojom.h"
#include "ash/public/interfaces/docked_magnifier_controller.mojom.h"
#include "ash/public/interfaces/event_rewriter_controller.mojom.h"
#include "ash/public/interfaces/first_run_helper.mojom.h"
#include "ash/public/interfaces/highlighter_controller.mojom.h"
#include "ash/public/interfaces/ime_controller.mojom.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "ash/public/interfaces/locale.mojom.h"
#include "ash/public/interfaces/login_screen.mojom.h"
#include "ash/public/interfaces/media.mojom.h"
#include "ash/public/interfaces/new_window.mojom.h"
#include "ash/public/interfaces/night_light_controller.mojom.h"
#include "ash/public/interfaces/note_taking_controller.mojom.h"
#include "ash/public/interfaces/pref_connector.mojom.h"
#include "ash/public/interfaces/process_creation_time_recorder.mojom.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/public/interfaces/shutdown.mojom.h"
#include "ash/public/interfaces/split_view.mojom.h"
#include "ash/public/interfaces/system_tray.mojom.h"
#include "ash/public/interfaces/tablet_mode.mojom.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/public/interfaces/vpn_list.mojom.h"
#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/no_destructor.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "services/ws/public/cpp/manifest.h"
#include "services/ws/public/mojom/constants.mojom.h"

namespace ash {

service_manager::Manifest& GetAmendmentForTesting() {
  static base::NoDestructor<service_manager::Manifest> amendment;
  return *amendment;
}

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Ash Window Manager and Shell")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSingleton)
                           .Build())
          .ExposeCapability(
              "system_ui",
              service_manager::Manifest::InterfaceList<
                  mojom::AcceleratorController, mojom::AccessibilityController,
                  mojom::AccessibilityFocusRingController,
                  mojom::AppListController, mojom::ArcCustomTabController,
                  mojom::AshMessageCenterController,
                  mojom::AssistantAlarmTimerController,
                  mojom::AssistantController,
                  mojom::AssistantNotificationController,
                  mojom::AssistantScreenContextController,
                  mojom::AssistantSetupController,
                  mojom::AssistantVolumeControl, mojom::CastConfig,
                  mojom::KioskNextShellController,
                  mojom::CrosDisplayConfigController,
                  mojom::DockedMagnifierController,
                  mojom::EventRewriterController, mojom::FirstRunHelper,
                  mojom::HighlighterController, mojom::ImeController,
                  mojom::KeyboardController, mojom::LocaleUpdateController,
                  mojom::LoginScreen, mojom::MediaController,
                  mojom::NewWindowController, mojom::NightLightController,
                  mojom::NoteTakingController,
                  mojom::ProcessCreationTimeRecorder, mojom::SessionController,
                  mojom::ShelfController, mojom::ShutdownController,
                  mojom::SplitViewController, mojom::SystemTray,
                  mojom::TabletModeController, mojom::TrayAction,
                  mojom::VoiceInteractionController, mojom::VpnList,
                  mojom::WallpaperController>())
          .ExposeCapability("display", service_manager::Manifest::InterfaceList<
                                           mojom::AshDisplayController,
                                           mojom::DisplayOutputProtection>())
          .RequireCapability("*", "accessibility")
          .RequireCapability("*", "app")
          .RequireCapability(prefs::mojom::kLocalStateServiceName,
                             "pref_client")
          .RequireCapability(content::mojom::kServiceName, "navigation")
          .RequireCapability(data_decoder::mojom::kServiceName, "image_decoder")
          .RequireCapability(mojom::kPrefConnectorServiceName, "pref_connector")
          .RequireCapability(viz::mojom::kVizServiceName, "ozone")
          .RequireCapability(viz::mojom::kVizServiceName, "viz_host")
          .RequireCapability(ws::mojom::kServiceName, "ozone")
          .RequireCapability(ws::mojom::kServiceName, "window_manager")
          .RequireCapability(device::mojom::kServiceName,
                             "device:bluetooth_system")
          .RequireCapability(device::mojom::kServiceName, "device:fingerprint")
          .RequireCapability(chromeos::multidevice_setup::mojom::kServiceName,
                             "multidevice_setup")
          .PackageService(ws::GetManifest())
          .Build()
          .Amend(GetAmendmentForTesting())};
  return *manifest;
}

void AmendManifestForTesting(const service_manager::Manifest& amendment) {
  GetAmendmentForTesting() = amendment;
}

}  // namespace ash
