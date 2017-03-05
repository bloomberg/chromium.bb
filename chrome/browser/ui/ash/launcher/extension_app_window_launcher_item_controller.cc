// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"

#include "ash/common/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image.h"
#include "ui/wm/core/window_animations.h"

ExtensionAppWindowLauncherItemController::
    ExtensionAppWindowLauncherItemController(
        const std::string& app_id,
        const std::string& launch_id,
        ChromeLauncherController* controller)
    : AppWindowLauncherItemController(app_id, launch_id, controller) {}

ExtensionAppWindowLauncherItemController::
    ~ExtensionAppWindowLauncherItemController() {}

void ExtensionAppWindowLauncherItemController::AddAppWindow(
    extensions::AppWindow* app_window) {
  DCHECK(!app_window->window_type_is_panel());
  AddWindow(app_window->GetBaseWindow());
}

ash::ShelfAppMenuItemList
ExtensionAppWindowLauncherItemController::GetAppMenuItems(int event_flags) {
  ash::ShelfAppMenuItemList items;
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(launcher_controller()->profile());

  uint32_t window_index = 0;
  for (const ui::BaseWindow* window : windows()) {
    extensions::AppWindow* app_window =
        app_window_registry->GetAppWindowForNativeWindow(
            window->GetNativeWindow());
    DCHECK(app_window);

    // Use the app's web contents favicon, or the app window's icon.
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(
            app_window->web_contents());
    gfx::Image icon = favicon_driver->GetFavicon();
    if (icon.IsEmpty())
      icon = app_window->app_icon();

    items.push_back(base::MakeUnique<ash::ShelfApplicationMenuItem>(
        window_index, app_window->GetTitle(), &icon));
    ++window_index;
  }
  return items;
}

void ExtensionAppWindowLauncherItemController::ExecuteCommand(
    uint32_t command_id,
    int event_flags) {
  launcher_controller()->ActivateShellApp(app_id(), command_id);
}
