// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"

#include <string>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "chrome/browser/ui/app_list/crostini/crostini_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_item_controller.h"
#include "components/exo/shell_surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kArcAppIdPrefix[] = "org.chromium.arc";

// Please note the down cast from ShelfItemDelegate to
// CrostiniAppWindowShelfItemController.
CrostiniAppWindowShelfItemController* GetItemController(
    const ash::ShelfID& shelf_id,
    const ash::ShelfModel& model) {
  DCHECK(IsCrostiniAppId(shelf_id.app_id));

  ash::ShelfItemDelegate* item_delegate = model.GetShelfItemDelegate(shelf_id);
  if (item_delegate == nullptr)
    return nullptr;
  return static_cast<CrostiniAppWindowShelfItemController*>(
      item_delegate->AsAppWindowLauncherItemController());
}

}  // namespace

CrostiniAppWindowShelfController::CrostiniAppWindowShelfController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

CrostiniAppWindowShelfController::~CrostiniAppWindowShelfController() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void CrostiniAppWindowShelfController::OnWindowInitialized(
    aura::Window* window) {
  // An Crostini window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;
  observed_windows_.push_back(window);

  window->AddObserver(this);
}

void CrostiniAppWindowShelfController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (!visible)
    return;

  const std::string* window_app_id =
      exo::ShellSurface::GetApplicationId(window);
  if (window_app_id == nullptr)
    return;

  // Skip handling ARC++ windows.
  if (strncmp(window_app_id->c_str(), kArcAppIdPrefix,
              sizeof(kArcAppIdPrefix)) == 0)
    return;

  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  RegisterAppWindow(window, window_app_id);
}

void CrostiniAppWindowShelfController::RegisterAppWindow(
    aura::Window* window,
    const std::string* window_app_id) {
  const std::string crostini_app_id = CreateCrostiniAppId(*window_app_id);
  const ash::ShelfID shelf_id(crostini_app_id);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  aura_window_to_app_window_[window] =
      std::make_unique<CrostiniAppWindow>(shelf_id, widget);
  CrostiniAppWindow* app_window = aura_window_to_app_window_[window].get();

  CrostiniAppWindowShelfItemController* item_controller =
      GetItemController(shelf_id, *owner()->shelf_model());
  if (item_controller == nullptr) {
    std::unique_ptr<CrostiniAppWindowShelfItemController> controller =
        std::make_unique<CrostiniAppWindowShelfItemController>(shelf_id);
    item_controller = controller.get();
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller),
                                     ash::STATUS_RUNNING);
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
      owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
    }
    window->SetProperty(ash::kShelfIDKey,
                        new std::string(shelf_id.Serialize()));
  }

  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void CrostiniAppWindowShelfController::OnWindowDestroying(
    aura::Window* window) {
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;
  UnregisterAppWindow(app_window_it->second.get());

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  CrostiniAppWindowShelfItemController* item_controller = GetItemController(
      app_window_it->second->shelf_id(), *owner()->shelf_model());
  if (item_controller != nullptr && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());

  aura_window_to_app_window_.erase(app_window_it);
}

AppWindowLauncherItemController*
CrostiniAppWindowShelfController::ControllerForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return nullptr;

  CrostiniAppWindow* app_window = app_window_it->second.get();

  if (app_window == nullptr)
    return nullptr;

  return app_window->controller();
}

void CrostiniAppWindowShelfController::UnregisterAppWindow(
    CrostiniAppWindow* app_window) {
  if (!app_window)
    return;

  CrostiniAppWindowShelfItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);
  app_window->SetController(nullptr);
}

void CrostiniAppWindowShelfController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    CrostiniAppWindow* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}
