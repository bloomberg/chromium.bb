// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/extension.h"
#include "ui/aura/client/activation_client.h"

namespace {

std::string GetAppLauncherId(ShellWindow* shell_window) {
  if (shell_window->window_type() == ShellWindow::WINDOW_TYPE_PANEL)
    return StringPrintf("panel:%d", shell_window->session_id().id());
  return shell_window->extension()->id();
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

// AppLauncherItemController ---------------------------------------------------

// This is a LauncherItemController for shell windows. There is one instance
// per app, per launcher id. [[[Currently, apps can not define multiple launcher
// ids (see GatAppLauncherId() above) so there is one instance per app]]].
// For apps with multiple windows, each item controller keeps track of all
// windows associated with the app and their activation order.
// Instances are owned by ShellWindowLauncherController.
class ShellWindowLauncherController::AppLauncherItemController
    : public LauncherItemController {
 public:
  AppLauncherItemController(const std::string& app_launcher_id,
                            const std::string& app_id,
                            ChromeLauncherController* controller) :
      LauncherItemController(TYPE_APP, app_id, controller),
      app_launcher_id_(app_launcher_id) {
  }
  virtual ~AppLauncherItemController() {}

  void AddShellWindow(ShellWindow* shell_window,
                      ash::LauncherItemStatus status) {
    if (status == ash::STATUS_ACTIVE)
      shell_windows_.push_front(shell_window);
    else
      shell_windows_.push_back(shell_window);
  }

  void RemoveShellWindowForWindow(aura::Window* window) {
    ShellWindowList::iterator iter =
        std::find_if(shell_windows_.begin(), shell_windows_.end(),
                     ShellWindowHasWindow(window));
    if (iter != shell_windows_.end())
      shell_windows_.erase(iter);
  }

  void SetActiveWindow(aura::Window* window) {
    ShellWindowList::iterator iter =
        std::find_if(shell_windows_.begin(), shell_windows_.end(),
                     ShellWindowHasWindow(window));
    if (iter != shell_windows_.end()) {
      ShellWindow* shell_window = *iter;
      shell_windows_.erase(iter);
      shell_windows_.push_front(shell_window);
    }
  }

  const std::string& app_launcher_id() const { return app_launcher_id_; }

  virtual string16 GetTitle() OVERRIDE {
    return GetAppTitle();
  }

  virtual bool HasWindow(aura::Window* window) const OVERRIDE {
    ShellWindowList::const_iterator iter =
        std::find_if(shell_windows_.begin(), shell_windows_.end(),
                     ShellWindowHasWindow(window));
    return iter != shell_windows_.end();
  }

  virtual bool IsOpen() const OVERRIDE {
    return !shell_windows_.empty();
  }

  virtual void Launch(int event_flags) OVERRIDE {
    launcher_controller()->LaunchApp(app_id(), ui::EF_NONE);
  }

  virtual void Activate() OVERRIDE {
    DCHECK(!shell_windows_.empty());
    shell_windows_.front()->GetBaseWindow()->Activate();
  }

  virtual void Close() OVERRIDE {
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
  virtual void Clicked() OVERRIDE {
    if (shell_windows_.empty())
      return;
    ShellWindow* first_window = shell_windows_.front();
    if (shell_windows_.size() == 1) {
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

  virtual void OnRemoved() OVERRIDE {
  }

  virtual void LauncherItemChanged(int model_index,
                                   const ash::LauncherItem& old_item) OVERRIDE {
  }

  size_t shell_window_count() const { return shell_windows_.size(); }

 private:
  typedef std::list<ShellWindow*> ShellWindowList;

  void RestoreOrShow(ShellWindow* shell_window) {
    if (shell_window->GetBaseWindow()->IsMinimized())
      shell_window->GetBaseWindow()->Restore();
    else
      shell_window->GetBaseWindow()->Show();
    // Always activate windows when shown from the launcher.
    shell_window->GetBaseWindow()->Activate();
  }

  // List of associated shell windows in activation order.
  ShellWindowList shell_windows_;

  // The launcher id associated with this set of windows. There is one
  // AppLauncherItemController for each |app_launcher_id_|.
  const std::string app_launcher_id_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherItemController);
};

// ShellWindowLauncherController -----------------------------------------------

ShellWindowLauncherController::ShellWindowLauncherController(
    ChromeLauncherController* owner)
    : ActivationChangeShim(ash::Shell::HasInstance() ?
          ash::Shell::GetInstance() : NULL),
      owner_(owner),
      registry_(extensions::ShellWindowRegistry::Get(owner->profile())),
      activation_client_(NULL) {
  registry_->AddObserver(this);
  if (ash::Shell::HasInstance()) {
    if (ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
      activation_client_ = aura::client::GetActivationClient(
          ash::Shell::GetInstance()->GetPrimaryRootWindow());
      if (activation_client_)
        activation_client_->AddObserver(this);
    }
  }
}


ShellWindowLauncherController::~ShellWindowLauncherController() {
  registry_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  for (WindowToAppLauncherIdMap::iterator iter =
           window_to_app_launcher_id_map_.begin();
       iter != window_to_app_launcher_id_map_.end(); ++iter) {
    iter->first->RemoveObserver(this);
  }
  STLDeleteContainerPairSecondPointers(
      app_controller_map_.begin(), app_controller_map_.end());
}

void ShellWindowLauncherController::OnShellWindowAdded(
    ShellWindow* shell_window) {
  aura::Window* window = shell_window->GetNativeWindow();
  // Get the app's launcher identifier and add an entry to the map.
  DCHECK(window_to_app_launcher_id_map_.find(window) ==
         window_to_app_launcher_id_map_.end());
  const std::string app_launcher_id = GetAppLauncherId(shell_window);
  window_to_app_launcher_id_map_[window] = app_launcher_id;
  window->AddObserver(this);

  // Find or create an item controller and launcher item.
  std::string app_id = shell_window->extension()->id();
  ash::LauncherItemStatus status = ash::wm::IsActiveWindow(window) ?
      ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
  AppControllerMap::iterator iter = app_controller_map_.find(app_launcher_id);
  ash::LauncherID launcher_id = 0;
  if (iter != app_controller_map_.end()) {
    AppLauncherItemController* controller = iter->second;
    DCHECK(controller->app_id() == app_id);
    launcher_id = controller->launcher_id();
    controller->AddShellWindow(shell_window, status);
  } else {
    AppLauncherItemController* controller =
        new AppLauncherItemController(app_launcher_id, app_id, owner_);
    controller->AddShellWindow(shell_window, status);
    // If the app launcher id is not unique, and there is already a launcher
    // item for this app id (e.g. pinned), use that launcher item.
    if (app_launcher_id == app_id)
      launcher_id = owner_->GetLauncherIDForAppID(app_id);
    if (launcher_id == 0)
      launcher_id = owner_->CreateAppLauncherItem(controller, app_id, status);
    else
      owner_->SetItemController(launcher_id, controller);
    const std::string app_launcher_id = GetAppLauncherId(shell_window);
    app_controller_map_[app_launcher_id] = controller;
  }
  owner_->SetItemStatus(launcher_id, status);
}

void ShellWindowLauncherController::OnShellWindowIconChanged(
    ShellWindow* shell_window) {
  // TODO(stevenjb): Fetch and set the launcher icon using
  // shell_window->app_icon_url().
}

void ShellWindowLauncherController::OnShellWindowRemoved(
    ShellWindow* shell_window) {
  // Do nothing here; shell_window->window() has allready been deleted and
  // OnWindowDestroying() has been called, doing the removal.
}

// Called from ~aura::Window(), before delegate_->OnWindowDestroyed() which
// destroys ShellWindow, so both |window| and the associated ShellWindow
// are valid here.
void ShellWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  WindowToAppLauncherIdMap::iterator iter1 =
      window_to_app_launcher_id_map_.find(window);
  DCHECK(iter1 != window_to_app_launcher_id_map_.end());
  std::string app_launcher_id = iter1->second;
  window_to_app_launcher_id_map_.erase(iter1);
  window->RemoveObserver(this);

  AppControllerMap::iterator iter2 = app_controller_map_.find(app_launcher_id);
  DCHECK(iter2 != app_controller_map_.end());
  AppLauncherItemController* controller = iter2->second;
  controller->RemoveShellWindowForWindow(window);
  if (controller->shell_window_count() == 0) {
    // If this is the last window associated with the app launcher id, close the
    // launcher item.
    ash::LauncherID launcher_id = controller->launcher_id();
    owner_->CloseLauncherItem(launcher_id);
    app_controller_map_.erase(iter2);
    delete controller;
  }
}

void ShellWindowLauncherController::OnWindowActivated(
    aura::Window* new_active,
    aura::Window* old_active) {
  // Make the newly active window the active (first) entry in the controlelr.
  AppLauncherItemController* new_controller = ControllerForWindow(new_active);
  if (new_controller) {
    new_controller->SetActiveWindow(new_active);
    owner_->SetItemStatus(new_controller->launcher_id(), ash::STATUS_ACTIVE);
  }

  // Mark the old active window's launcher item as running (if different).
  AppLauncherItemController* old_controller = ControllerForWindow(old_active);
  if (old_controller && old_controller != new_controller)
    owner_->SetItemStatus(old_controller->launcher_id(), ash::STATUS_RUNNING);
}

// Private Methods

ShellWindowLauncherController::AppLauncherItemController*
ShellWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  WindowToAppLauncherIdMap::iterator iter1 =
      window_to_app_launcher_id_map_.find(window);
  if (iter1 == window_to_app_launcher_id_map_.end())
    return NULL;
  std::string app_launcher_id = iter1->second;
  AppControllerMap::iterator iter2 = app_controller_map_.find(app_launcher_id);
  if (iter2 == app_controller_map_.end())
    return NULL;
  return iter2->second;
}
