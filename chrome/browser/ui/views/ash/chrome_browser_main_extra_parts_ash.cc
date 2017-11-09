// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/public/cpp/mus_property_mirror_ash.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/cast_config_client_media_router.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/chrome_shell_content_state.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/browser/ui/ash/lock_screen_client.h"
#include "chrome/browser/ui/ash/media_client.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/volume_controller.h"
#include "chrome/browser/ui/ash/vpn_list_forwarder.h"
#include "chrome/browser/ui/views/ash/tab_scrubber.h"
#include "chrome/browser/ui/views/frame/immersive_context_mus.h"
#include "chrome/browser/ui/views/frame/immersive_handler_factory_mus.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/user_activity_monitor.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/user_activity_forwarder.h"
#include "ui/base/class_property.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/keyboard/content/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/mus/mus_client.h"

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
#include "chrome/browser/exo_parts.h"
#endif

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {}

void ChromeBrowserMainExtraPartsAsh::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  if (ash_util::IsRunningInMash()) {
    // ash::Shell will not be created because ash is running out-of-process.
    ash::Shell::SetIsBrowserProcessWithMash();

    // Register ash-specific window properties with Chrome's property converter.
    // This propagates ash properties set on chrome windows to ash, via mojo.
    DCHECK(views::MusClient::Exists());
    views::MusClient* mus_client = views::MusClient::Get();
    aura::WindowTreeClientDelegate* delegate = mus_client;
    aura::PropertyConverter* converter = delegate->GetPropertyConverter();

    converter->RegisterPrimitiveProperty(
        ash::kCanConsumeSystemKeysKey,
        ash::mojom::kCanConsumeSystemKeys_Property,
        aura::PropertyConverter::CreateAcceptAnyValueCallback());
    converter->RegisterPrimitiveProperty(
        ash::kPanelAttachedKey,
        ui::mojom::WindowManager::kPanelAttached_Property,
        aura::PropertyConverter::CreateAcceptAnyValueCallback());
    converter->RegisterPrimitiveProperty(
        ash::kShelfItemTypeKey,
        ui::mojom::WindowManager::kShelfItemType_Property,
        base::Bind(&ash::IsValidShelfItemType));
    converter->RegisterPrimitiveProperty(
        ash::kWindowStateTypeKey, ash::mojom::kWindowStateType_Property,
        base::Bind(&ash::IsValidWindowStateType));
    converter->RegisterPrimitiveProperty(
        ash::kWindowPinTypeKey, ash::mojom::kWindowPinType_Property,
        base::Bind(&ash::IsValidWindowPinType));
    converter->RegisterPrimitiveProperty(
        ash::kWindowPositionManagedTypeKey,
        ash::mojom::kWindowPositionManaged_Property,
        aura::PropertyConverter::CreateAcceptAnyValueCallback());
    converter->RegisterStringProperty(
        ash::kShelfIDKey, ui::mojom::WindowManager::kShelfID_Property);

    mus_client->SetMusPropertyMirror(
        base::MakeUnique<ash::MusPropertyMirrorAsh>());
  }
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (ash_util::ShouldOpenAshOnStartup())
    ash_init_ = base::MakeUnique<AshInit>();

  if (ash_util::IsRunningInMash()) {
    immersive_context_ = base::MakeUnique<ImmersiveContextMus>();
    immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryMus>();

    // Enterprise support in the browser can monitor user activity. Connect to
    // the UI service to monitor activity. The ash process has its own monitor.
    user_activity_detector_ = base::MakeUnique<ui::UserActivityDetector>();
    ui::mojom::UserActivityMonitorPtr user_activity_monitor;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ui::mojom::kServiceName, &user_activity_monitor);
    user_activity_forwarder_ = base::MakeUnique<aura::UserActivityForwarder>(
        std::move(user_activity_monitor), user_activity_detector_.get());
  }

  session_controller_client_ = base::MakeUnique<SessionControllerClient>();
  session_controller_client_->Init();

  // Must be available at login screen, so initialize before profile.
  system_tray_client_ = base::MakeUnique<SystemTrayClient>();
  ime_controller_client_ = base::MakeUnique<ImeControllerClient>(
      chromeos::input_method::InputMethodManager::Get());
  ime_controller_client_->Init();
  new_window_client_ = base::MakeUnique<ChromeNewWindowClient>();
  volume_controller_ = base::MakeUnique<VolumeController>();
  vpn_list_forwarder_ = base::MakeUnique<VpnListForwarder>();

  // For OS_CHROMEOS, virtual keyboard needs to be initialized before profile
  // initialized. Otherwise, virtual keyboard extension will not load at login
  // screen.
  keyboard::InitializeKeyboard();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  exo_parts_ = ExoParts::CreateIfNecessary();
#endif
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  if (ash_util::IsRunningInMash())
    chrome_shell_content_state_ = base::MakeUnique<ChromeShellContentState>();

  cast_config_client_media_router_ =
      base::MakeUnique<CastConfigClientMediaRouter>();
  media_client_ = base::MakeUnique<MediaClient>();
  lock_screen_client_ = base::MakeUnique<LockScreenClient>();

  // TODO(mash): Port TabScrubber and keyboard initialization.
  if (ash_util::IsRunningInMash())
    return;

  // Initialize TabScrubber after the Ash Shell has been initialized.
  TabScrubber::GetInstance();

  // Activate virtual keyboard after profile is initialized. It depends on the
  // default profile.
  ash::Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  // ExoParts uses state from ash, delete it before ash so that exo can
  // uninstall correctly.
  exo_parts_.reset();
#endif

  vpn_list_forwarder_.reset();
  volume_controller_.reset();
  new_window_client_.reset();
  ime_controller_client_.reset();
  system_tray_client_.reset();
  lock_screen_client_.reset();
  media_client_.reset();
  cast_config_client_media_router_.reset();
  session_controller_client_.reset();

  ash_init_.reset();
}
