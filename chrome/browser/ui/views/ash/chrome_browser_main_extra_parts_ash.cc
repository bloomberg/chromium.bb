// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/cast_config_client_media_router.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/chrome_shell_content_state.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_mus.h"
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
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/base/class_property.h"
#include "ui/keyboard/content/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_property_mirror.h"

namespace {

void MirrorIcon(aura::Window* window,
                aura::Window* root_window,
                const aura::WindowProperty<gfx::ImageSkia*>* key) {
  gfx::ImageSkia* value = window->GetProperty(key);
  if (!value || value->isNull())
    root_window->ClearProperty(key);
  else
    root_window->SetProperty(key, new gfx::ImageSkia(*value));
}

}  // namespace

// Relays aura content window properties to its root window (the mash frame).
// Ash relies on various window properties for frame titles, shelf items, etc.
// These properties are read from the client's root, not child content windows.
class MusPropertyMirrorAsh : public views::MusPropertyMirror {
 public:
  MusPropertyMirrorAsh() {}
  ~MusPropertyMirrorAsh() override {}

  // MusPropertyMirror:
  void MirrorPropertyFromWidgetWindowToRootWindow(aura::Window* window,
                                                  aura::Window* root_window,
                                                  const void* key) override {
    if (key == ash::kShelfIDKey) {
      int32_t value = window->GetProperty(ash::kShelfIDKey);
      root_window->SetProperty(ash::kShelfIDKey, value);
    } else if (key == ash::kShelfItemTypeKey) {
      int32_t value = window->GetProperty(ash::kShelfItemTypeKey);
      root_window->SetProperty(ash::kShelfItemTypeKey, value);
    } else if (key == aura::client::kAppIconKey) {
      MirrorIcon(window, root_window, aura::client::kAppIconKey);
    } else if (key == aura::client::kAppIdKey) {
      std::string* value = window->GetProperty(aura::client::kAppIdKey);
      root_window->SetProperty(aura::client::kAppIdKey, value);
    } else if (key == aura::client::kDrawAttentionKey) {
      bool value = window->GetProperty(aura::client::kDrawAttentionKey);
      root_window->SetProperty(aura::client::kDrawAttentionKey, value);
    } else if (key == aura::client::kTitleKey) {
      base::string16* value = window->GetProperty(aura::client::kTitleKey);
      root_window->SetProperty(aura::client::kTitleKey, value);
    } else if (key == aura::client::kWindowIconKey) {
      MirrorIcon(window, root_window, aura::client::kWindowIconKey);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MusPropertyMirrorAsh);
};

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {}

void ChromeBrowserMainExtraPartsAsh::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
  if (chrome::IsRunningInMash()) {
    // Register ash-specific window properties with Chrome's property converter.
    // This propagates ash properties set on chrome windows to ash, via mojo.
    DCHECK(views::MusClient::Exists());
    views::MusClient* mus_client = views::MusClient::Get();
    aura::WindowTreeClientDelegate* delegate = mus_client;
    aura::PropertyConverter* converter = delegate->GetPropertyConverter();
    converter->RegisterProperty(
        ash::kPanelAttachedKey,
        ui::mojom::WindowManager::kPanelAttached_Property);
    converter->RegisterProperty(
        ash::kShelfItemTypeKey,
        ui::mojom::WindowManager::kShelfItemType_Property);

    mus_client->SetMusPropertyMirror(base::MakeUnique<MusPropertyMirrorAsh>());
  }
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (chrome::ShouldOpenAshOnStartup())
    chrome::OpenAsh(gfx::kNullAcceleratedWidget);

  if (chrome::IsRunningInMash()) {
    immersive_context_ = base::MakeUnique<ImmersiveContextMus>();
    immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryMus>();
  }

  session_controller_client_ = base::MakeUnique<SessionControllerClient>();

  // Must be available at login screen, so initialize before profile.
  system_tray_client_ = base::MakeUnique<SystemTrayClient>();
  new_window_client_ = base::MakeUnique<ChromeNewWindowClient>();
  volume_controller_ = base::MakeUnique<VolumeController>();
  vpn_list_forwarder_ = base::MakeUnique<VpnListForwarder>();

  // For OS_CHROMEOS, virtual keyboard needs to be initialized before profile
  // initialized. Otherwise, virtual keyboard extension will not load at login
  // screen.
  keyboard::InitializeKeyboard();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  if (chrome::IsRunningInMash()) {
    DCHECK(!ash::Shell::HasInstance());
    DCHECK(!ChromeLauncherController::instance());
    chrome_launcher_controller_mus_ =
        base::MakeUnique<ChromeLauncherControllerMus>();
    chrome_launcher_controller_mus_->Init();
    chrome_shell_content_state_ = base::MakeUnique<ChromeShellContentState>();
  }

  cast_config_client_media_router_ =
      base::MakeUnique<CastConfigClientMediaRouter>();
  media_client_ = base::MakeUnique<MediaClient>();

  if (!ash::Shell::HasInstance())
    return;

  // Initialize TabScrubber after the Ash Shell has been initialized.
  TabScrubber::GetInstance();
  // Activate virtual keyboard after profile is initialized. It depends on the
  // default profile.
  ash::Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  vpn_list_forwarder_.reset();
  volume_controller_.reset();
  new_window_client_.reset();
  system_tray_client_.reset();
  media_client_.reset();
  cast_config_client_media_router_.reset();
  session_controller_client_.reset();

  chrome::CloseAsh();
}
