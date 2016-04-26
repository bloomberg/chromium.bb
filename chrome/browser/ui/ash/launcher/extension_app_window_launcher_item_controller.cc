// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"

#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "skia/ext/image_operations.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/window_animations.h"

namespace {

// Size of the icon in the shelf launcher in display-independent pixels.
const int kAppListIconSize = 24;

// This will return a slightly smaller icon than the app icon to be used in
// the application list menu.
gfx::Image GetAppListIcon(extensions::AppWindow* app_window) {
  // TODO(skuhne): We instead might want to use LoadImages in
  // AppWindow::UpdateExtensionAppIcon() to let the extension give us
  // pre-defined icons in the launcher and the launcher list sizes. Since there
  // is no mock yet, doing this now seems a bit premature and we scale for the
  // time being.
  if (app_window->app_icon().IsEmpty())
    return gfx::Image();

  SkBitmap bmp = skia::ImageOperations::Resize(
      *app_window->app_icon().ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
      kAppListIconSize, kAppListIconSize);
  return gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bmp));
}

}  // namespace

ExtensionAppWindowLauncherItemController::
    ExtensionAppWindowLauncherItemController(
        Type type,
        const std::string& app_shelf_id,
        const std::string& app_id,
        ChromeLauncherController* controller)
    : AppWindowLauncherItemController(type, app_shelf_id, app_id, controller) {}

ExtensionAppWindowLauncherItemController::
    ~ExtensionAppWindowLauncherItemController() {}

void ExtensionAppWindowLauncherItemController::AddAppWindow(
    extensions::AppWindow* app_window) {
  if (app_window->window_type_is_panel() && type() != TYPE_APP_PANEL)
    LOG(ERROR) << "AppWindow of type Panel added to non-panel launcher item";
  DCHECK(window_to_app_window_.find(app_window->GetBaseWindow()) ==
         window_to_app_window_.end());
  AddWindow(app_window->GetBaseWindow());
  window_to_app_window_[app_window->GetBaseWindow()] = app_window;
}

void ExtensionAppWindowLauncherItemController::OnWindowRemoved(
    ui::BaseWindow* window) {
  WindowToAppWindow::iterator it = window_to_app_window_.find(window);
  if (it == window_to_app_window_.end()) {
    NOTREACHED();
    return;
  }

  window_to_app_window_.erase(it);
}

ChromeLauncherAppMenuItems
ExtensionAppWindowLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items =
      AppWindowLauncherItemController::GetApplicationList(event_flags);
  int index = 0;
  for (const auto* window : windows()) {
    extensions::AppWindow* app_window = window_to_app_window_[window];
    DCHECK(app_window);

    // If the app's web contents provides a favicon, use it. Otherwise, use a
    // scaled down app icon.
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(
            app_window->web_contents());
    gfx::Image result = favicon_driver->GetFavicon();
    if (result.IsEmpty())
      result = GetAppListIcon(app_window);

    items.push_back(new ChromeLauncherAppMenuItemV2App(
        app_window->GetTitle(),
        &result,  // Will be copied
        app_id(), launcher_controller(), index,
        index == 0 /* has_leading_separator */));
    ++index;
  }
  return items;
}

ash::ShelfItemDelegate::PerformedAction
ExtensionAppWindowLauncherItemController::ItemSelected(const ui::Event& event) {
  if (windows().empty())
    return kNoAction;

  if (type() == TYPE_APP_PANEL) {
    DCHECK_EQ(windows().size(), 1u);
    ui::BaseWindow* panel = windows().front();
    aura::Window* panel_window = panel->GetNativeWindow();
    // If the panel is attached on another display, move it to the current
    // display and activate it.
    if (ash::wm::GetWindowState(panel_window)->panel_attached() &&
        ash::wm::MoveWindowToEventRoot(panel_window, event)) {
      if (!panel->IsActive())
        return ShowAndActivateOrMinimize(panel);
    } else {
      return ShowAndActivateOrMinimize(panel);
    }
  } else {
    return AppWindowLauncherItemController::ItemSelected(event);
  }
  return kNoAction;
}

base::string16 ExtensionAppWindowLauncherItemController::GetTitle() {
  // For panels return the title of the contents if set.
  // Otherwise return the title of the app.
  if (type() == TYPE_APP_PANEL && !windows().empty()) {
    extensions::AppWindow* app_window =
        window_to_app_window_[windows().front()];
    DCHECK(app_window);
    if (app_window->web_contents()) {
      base::string16 title = app_window->web_contents()->GetTitle();
      if (!title.empty())
        return title;
    }
  }
  return AppWindowLauncherItemController::GetTitle();
}

ash::ShelfMenuModel*
ExtensionAppWindowLauncherItemController::CreateApplicationMenu(
    int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

bool ExtensionAppWindowLauncherItemController::IsDraggable() {
  if (type() == TYPE_APP_PANEL)
    return true;
  return AppWindowLauncherItemController::IsDraggable();
}

bool ExtensionAppWindowLauncherItemController::ShouldShowTooltip() {
  if (type() == TYPE_APP_PANEL && IsVisible())
    return false;
  return AppWindowLauncherItemController::ShouldShowTooltip();
}
