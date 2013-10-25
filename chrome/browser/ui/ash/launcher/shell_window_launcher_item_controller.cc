// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_item_controller.h"

#include "apps/shell_window.h"
#include "apps/ui/native_app_window.h"
#include "ash/launcher/launcher_model.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/corewm/window_animations.h"

using apps::ShellWindow;

namespace {

// Size of the icon in the shelf launcher in display-independent pixels.
const int kAppListIconSize = 24;

// This will return a slightly smaller icon than the app icon to be used in
// the application list menu.
scoped_ptr<gfx::Image> GetAppListIcon(ShellWindow* shell_window) {
  // TODO(skuhne): We instead might want to use LoadImages in
  // ShellWindow::UpdateExtensionAppIcon() to let the extension give us
  // pre-defined icons in the launcher and the launcher list sizes. Since there
  // is no mock yet, doing this now seems a bit premature and we scale for the
  // time being.
  if (shell_window->app_icon().IsEmpty())
    return make_scoped_ptr(new gfx::Image());

  SkBitmap bmp =
      skia::ImageOperations::Resize(*shell_window->app_icon().ToSkBitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    kAppListIconSize,
                                    kAppListIconSize);
  return make_scoped_ptr(
      new gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bmp)));
}

// Functor for std::find_if used in AppLauncherItemController.
class ShellWindowHasWindow {
 public:
  explicit ShellWindowHasWindow(aura::Window* window) : window_(window) { }

  bool operator()(ShellWindow* shell_window) const {
    return shell_window->GetNativeWindow() == window_;
  }

 private:
  const aura::Window* window_;
};

}  // namespace

ShellWindowLauncherItemController::ShellWindowLauncherItemController(
    Type type,
    const std::string& app_launcher_id,
    const std::string& app_id,
    ChromeLauncherController* controller)
    : LauncherItemController(type, app_id, controller),
      last_active_shell_window_(NULL),
      app_launcher_id_(app_launcher_id),
      observed_windows_(this) {
}

ShellWindowLauncherItemController::~ShellWindowLauncherItemController() {
}

void ShellWindowLauncherItemController::AddShellWindow(
    ShellWindow* shell_window,
    ash::LauncherItemStatus status) {
  if (shell_window->window_type_is_panel() && type() != TYPE_APP_PANEL)
    LOG(ERROR) << "ShellWindow of type Panel added to non-panel launcher item";
  shell_windows_.push_front(shell_window);
  observed_windows_.Add(shell_window->GetNativeWindow());
}

void ShellWindowLauncherItemController::RemoveShellWindowForWindow(
    aura::Window* window) {
  ShellWindowList::iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  if (iter != shell_windows_.end()) {
    if (*iter == last_active_shell_window_)
      last_active_shell_window_ = NULL;
    shell_windows_.erase(iter);
  }
  observed_windows_.Remove(window);
}

void ShellWindowLauncherItemController::SetActiveWindow(aura::Window* window) {
  ShellWindowList::iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  if (iter != shell_windows_.end())
    last_active_shell_window_ = *iter;
}

bool ShellWindowLauncherItemController::IsCurrentlyShownInWindow(
    aura::Window* window) const {
  ShellWindowList::const_iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  return iter != shell_windows_.end();
}

bool ShellWindowLauncherItemController::IsOpen() const {
  return !shell_windows_.empty();
}

bool ShellWindowLauncherItemController::IsVisible() const {
  // Return true if any windows are visible.
  for (ShellWindowList::const_iterator iter = shell_windows_.begin();
       iter != shell_windows_.end(); ++iter) {
    if ((*iter)->GetNativeWindow()->IsVisible())
      return true;
  }
  return false;
}

void ShellWindowLauncherItemController::Launch(ash::LaunchSource source,
                                               int event_flags) {
  launcher_controller()->LaunchApp(app_id(),
                                   source,
                                   ui::EF_NONE);
}

void ShellWindowLauncherItemController::Activate(ash::LaunchSource source) {
  DCHECK(!shell_windows_.empty());
  ShellWindow* window_to_activate = last_active_shell_window_ ?
      last_active_shell_window_ : shell_windows_.back();
  window_to_activate->GetBaseWindow()->Activate();
}

void ShellWindowLauncherItemController::Close() {
  // Note: Closing windows may affect the contents of shell_windows_.
  ShellWindowList windows_to_close = shell_windows_;
  for (ShellWindowList::iterator iter = windows_to_close.begin();
       iter != windows_to_close.end(); ++iter) {
    (*iter)->GetBaseWindow()->Close();
  }
}

void ShellWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= shell_windows_.size())
    return;
  ShellWindowList::iterator it = shell_windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it);
}

ChromeLauncherAppMenuItems
ShellWindowLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items;
  items.push_back(new ChromeLauncherAppMenuItem(GetTitle(), NULL, false));
  int index = 0;
  for (ShellWindowList::iterator iter = shell_windows_.begin();
       iter != shell_windows_.end(); ++iter) {
    ShellWindow* shell_window = *iter;
    scoped_ptr<gfx::Image> image(GetAppListIcon(shell_window));
    items.push_back(new ChromeLauncherAppMenuItemV2App(
        shell_window->GetTitle(),
        image.get(),  // Will be copied
        app_id(),
        launcher_controller(),
        index,
        index == 0 /* has_leading_separator */));
    ++index;
  }
  return items.Pass();
}

void ShellWindowLauncherItemController::ItemSelected(const ui::Event& event) {
  if (shell_windows_.empty())
    return;
  if (type() == TYPE_APP_PANEL) {
    DCHECK(shell_windows_.size() == 1);
    ShellWindow* panel = shell_windows_.front();
    aura::Window* panel_window = panel->GetNativeWindow();
    // If the panel is attached on another display, move it to the current
    // display and activate it.
    if (ash::wm::GetWindowState(panel_window)->panel_attached() &&
        ash::wm::MoveWindowToEventRoot(panel_window, event)) {
      if (!panel->GetBaseWindow()->IsActive())
        ShowAndActivateOrMinimize(panel);
    } else {
      ShowAndActivateOrMinimize(panel);
    }
  } else {
    ShellWindow* window_to_show = last_active_shell_window_ ?
        last_active_shell_window_ : shell_windows_.front();
    // If the event was triggered by a keystroke, we try to advance to the next
    // item if the window we are trying to activate is already active.
    if (shell_windows_.size() >= 1 &&
        window_to_show->GetBaseWindow()->IsActive() &&
        event.type() == ui::ET_KEY_RELEASED) {
      ActivateOrAdvanceToNextShellWindow(window_to_show);
    } else {
      ShowAndActivateOrMinimize(window_to_show);
    }
  }
}

base::string16 ShellWindowLauncherItemController::GetTitle() {
  // For panels return the title of the contents if set.
  // Otherwise return the title of the app.
  if (type() == TYPE_APP_PANEL && !shell_windows_.empty()) {
    ShellWindow* shell_window = shell_windows_.front();
    if (shell_window->web_contents()) {
      string16 title = shell_window->web_contents()->GetTitle();
      if (!title.empty())
        return title;
    }
  }
  return GetAppTitle();
}

ui::MenuModel* ShellWindowLauncherItemController::CreateContextMenu(
    aura::Window* root_window) {
  ash::LauncherItem item =
      *(launcher_controller()->model()->ItemByID(launcher_id()));
  return new LauncherContextMenu(launcher_controller(), &item, root_window);
}

ash::LauncherMenuModel*
ShellWindowLauncherItemController::CreateApplicationMenu(int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

bool ShellWindowLauncherItemController::IsDraggable() {
  if (type() == TYPE_APP_PANEL)
    return true;
  return launcher_controller()->CanPin() ? true : false;
}

bool ShellWindowLauncherItemController::ShouldShowTooltip() {
  if (type() == TYPE_APP_PANEL && IsVisible())
    return false;
  return true;
}

void ShellWindowLauncherItemController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == aura::client::kDrawAttentionKey) {
    ash::LauncherItemStatus status;
    if (ash::wm::IsActiveWindow(window)) {
      status = ash::STATUS_ACTIVE;
    } else if (window->GetProperty(aura::client::kDrawAttentionKey)) {
      status = ash::STATUS_ATTENTION;
    } else {
      status = ash::STATUS_RUNNING;
    }
    launcher_controller()->SetItemStatus(launcher_id(), status);
  }
}

void ShellWindowLauncherItemController::ShowAndActivateOrMinimize(
    ShellWindow* shell_window) {
  // Either show or minimize windows when shown from the launcher.
  launcher_controller()->ActivateWindowOrMinimizeIfActive(
      shell_window->GetBaseWindow(),
      GetApplicationList(0).size() == 2);
}

void ShellWindowLauncherItemController::ActivateOrAdvanceToNextShellWindow(
    ShellWindow* window_to_show) {
  ShellWindowList::iterator i(
      std::find(shell_windows_.begin(),
                shell_windows_.end(),
                window_to_show));
  if (i != shell_windows_.end()) {
    if (++i != shell_windows_.end())
      window_to_show = *i;
    else
      window_to_show = shell_windows_.front();
  }
  if (window_to_show->GetBaseWindow()->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    AnimateWindow(window_to_show->GetNativeWindow(),
                  views::corewm::WINDOW_ANIMATION_TYPE_BOUNCE);
  } else {
    ShowAndActivateOrMinimize(window_to_show);
  }
}
