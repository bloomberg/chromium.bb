// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_item_controller.h"

#include "ash/launcher/launcher_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"

namespace {

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
      app_launcher_id_(app_launcher_id) {
}

ShellWindowLauncherItemController::~ShellWindowLauncherItemController() {
}

void ShellWindowLauncherItemController::AddShellWindow(
    ShellWindow* shell_window,
    ash::LauncherItemStatus status) {
  if (shell_window->window_type_is_panel() && type() != TYPE_APP_PANEL) {
    LOG(ERROR) << "ShellWindow of type Panel added to non-panel launcher item";
  }
  if (status == ash::STATUS_ACTIVE)
    shell_windows_.push_front(shell_window);
  else
    shell_windows_.push_back(shell_window);
}

void ShellWindowLauncherItemController::RemoveShellWindowForWindow(
    aura::Window* window) {
  ShellWindowList::iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  if (iter != shell_windows_.end())
    shell_windows_.erase(iter);
}

void ShellWindowLauncherItemController::SetActiveWindow(
    aura::Window* window) {
  ShellWindowList::iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  if (iter != shell_windows_.end()) {
    ShellWindow* shell_window = *iter;
    shell_windows_.erase(iter);
    shell_windows_.push_front(shell_window);
  }
}

string16 ShellWindowLauncherItemController::GetTitle() {
  return GetAppTitle();
}

bool ShellWindowLauncherItemController::HasWindow(
    aura::Window* window) const {
  ShellWindowList::const_iterator iter =
      std::find_if(shell_windows_.begin(), shell_windows_.end(),
                   ShellWindowHasWindow(window));
  return iter != shell_windows_.end();
}

bool ShellWindowLauncherItemController::IsOpen() const {
  return !shell_windows_.empty();
}

void ShellWindowLauncherItemController::Launch(
    int event_flags) {
  launcher_controller()->LaunchApp(app_id(), ui::EF_NONE);
}

void ShellWindowLauncherItemController::Activate() {
  DCHECK(!shell_windows_.empty());
  shell_windows_.front()->GetBaseWindow()->Activate();
}

void ShellWindowLauncherItemController::Close() {
  // Note: Closing windows may affect the contents of shell_windows_.
  ShellWindowList windows_to_close = shell_windows_;
  for (ShellWindowList::iterator iter = windows_to_close.begin();
       iter != windows_to_close.end(); ++iter) {
    (*iter)->GetBaseWindow()->Close();
  }
}

// Behavior for app windows:
// * One window: Toggle minimization when clicked.
// * Multiple windows:
// ** If the first window is not active, activate it.
// ** Otherwise activate the next window.
void ShellWindowLauncherItemController::Clicked(const ui::Event& event) {
  if (shell_windows_.empty())
    return;
  ShellWindow* first_window = shell_windows_.front();
  if (shell_windows_.size() == 1) {
    ash::launcher::MoveToEventRootIfPanel(first_window->GetNativeWindow(),
                                          event);
    // If the window moves, it becomes inactive first then
    // gets activated in |RestoreOrShow| below.
    if (first_window->GetBaseWindow()->IsActive())
      first_window->GetBaseWindow()->Minimize();
    else
      RestoreOrShow(first_window);
  } else {
    if (!first_window->GetBaseWindow()->IsActive()) {
      RestoreOrShow(first_window);
    } else {
      shell_windows_.pop_front();
      shell_windows_.push_back(first_window);
      RestoreOrShow(shell_windows_.front());
    }
  }
}

void ShellWindowLauncherItemController::ActivateIndexedApp(
    size_t index) {
  if (index < shell_windows_.size()) {
    ShellWindowList::iterator it = shell_windows_.begin();
    std::advance(it, index);
    RestoreOrShow(*it);
  }
}

string16 ShellWindowLauncherItemController::GetTitleOfIndexedApp(
    size_t index) {
  if (index < shell_windows_.size()) {
    ShellWindowList::iterator it = shell_windows_.begin();
    std::advance(it, index);
    return (*it)->GetTitle();
  }
  return string16();
}

gfx::Image*
ShellWindowLauncherItemController::GetIconOfIndexedApp(size_t index) {
  if (index < shell_windows_.size()) {
    ShellWindowList::iterator it = shell_windows_.begin();
    std::advance(it, index);
    return (*it)->GetAppListIcon();
  }
  return new gfx::Image();
}

ChromeLauncherAppMenuItems
ShellWindowLauncherItemController::GetApplicationList() {
  ChromeLauncherAppMenuItems items;
  if (!launcher_controller()->GetPerAppInterface()) {
    items.push_back(new ChromeLauncherAppMenuItem(GetTitle(), NULL));
    for (size_t i = 0; i < shell_window_count(); i++) {
      gfx::Image* image = GetIconOfIndexedApp(i);
      items.push_back(new ChromeLauncherAppMenuItemV2App(
          GetTitleOfIndexedApp(i),
          image,
          app_id(),
          launcher_controller()->GetPerAppInterface(),
          i));
      delete image;
    }
  }
  return items.Pass();
}

void ShellWindowLauncherItemController::RestoreOrShow(
    ShellWindow* shell_window) {
  if (shell_window->GetBaseWindow()->IsMinimized())
    shell_window->GetBaseWindow()->Restore();
  else
    shell_window->GetBaseWindow()->Show();
  // Always activate windows when shown from the launcher.
  shell_window->GetBaseWindow()->Activate();
}
