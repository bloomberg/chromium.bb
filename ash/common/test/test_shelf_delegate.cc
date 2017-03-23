// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_delegate.h"

#include <utility>

#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_observer.h"
#include "ash/common/test/test_shelf_item_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

namespace {

// Set the |type| of the item with the given |shelf_id|, if the item exists.
void SetItemType(ShelfID shelf_id, ShelfItemType type) {
  ShelfModel* model = Shell::Get()->shelf_model();
  ash::ShelfItems::const_iterator item = model->ItemByID(shelf_id);
  if (item != model->items().end()) {
    ShelfItem pinned_item = *item;
    pinned_item.type = type;
    model->Set(item - model->items().begin(), pinned_item);
  }
}

}  // namespace

TestShelfDelegate* TestShelfDelegate::instance_ = nullptr;

// A ShellObserver that sets the shelf alignment and auto hide behavior when the
// shelf is created, to simulate ChromeLauncherController's behavior.
class ShelfInitializer : public ShellObserver {
 public:
  ShelfInitializer() { Shell::GetInstance()->AddShellObserver(this); }
  ~ShelfInitializer() override {
    Shell::GetInstance()->RemoveShellObserver(this);
  }

  // ShellObserver:
  void OnShelfCreatedForRootWindow(WmWindow* root_window) override {
    WmShelf* shelf = root_window->GetRootWindowController()->GetShelf();
    // Do not override the custom initialization performed by some unit tests.
    if (shelf->alignment() == SHELF_ALIGNMENT_BOTTOM_LOCKED &&
        shelf->auto_hide_behavior() == SHELF_AUTO_HIDE_ALWAYS_HIDDEN) {
      shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
      shelf->UpdateVisibilityState();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfInitializer);
};

TestShelfDelegate::TestShelfDelegate()
    : shelf_initializer_(base::MakeUnique<ShelfInitializer>()) {
  CHECK(!instance_);
  instance_ = this;
}

TestShelfDelegate::~TestShelfDelegate() {
  instance_ = nullptr;
}

void TestShelfDelegate::AddShelfItem(WmWindow* window) {
  AddShelfItem(window, std::string());
}

void TestShelfDelegate::AddShelfItem(WmWindow* window,
                                     const std::string& app_id) {
  ShelfItem item;
  if (!app_id.empty())
    item.app_launch_id = AppLaunchId(app_id);
  if (window->GetType() == ui::wm::WINDOW_TYPE_PANEL)
    item.type = TYPE_APP_PANEL;
  else
    item.type = TYPE_APP;
  ShelfModel* model = Shell::Get()->shelf_model();
  ShelfID id = model->next_id();
  item.status = STATUS_CLOSED;
  model->Add(item);
  window->aura_window()->AddObserver(this);

  model->SetShelfItemDelegate(id,
                              base::MakeUnique<TestShelfItemDelegate>(window));
  window->aura_window()->SetProperty(kShelfIDKey, id);
}

void TestShelfDelegate::RemoveShelfItemForWindow(WmWindow* window) {
  ShelfID shelf_id = window->aura_window()->GetProperty(kShelfIDKey);
  if (shelf_id == 0)
    return;
  ShelfModel* model = Shell::Get()->shelf_model();
  int index = model->ItemIndexByID(shelf_id);
  DCHECK_NE(-1, index);
  model->RemoveItemAt(index);
  window->aura_window()->RemoveObserver(this);
  const std::string& app_id = GetAppIDForShelfID(shelf_id);
  if (IsAppPinned(app_id))
    UnpinAppWithID(app_id);
}

void TestShelfDelegate::OnWindowDestroying(aura::Window* window) {
  RemoveShelfItemForWindow(WmWindow::Get(window));
}

void TestShelfDelegate::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  // The window may be legitimately reparented while staying open if it moves
  // to another display or container. If the window does not have a new parent
  // then remove the shelf item.
  if (!params.new_parent)
    RemoveShelfItemForWindow(WmWindow::Get(params.target));
}

ShelfID TestShelfDelegate::GetShelfIDForAppID(const std::string& app_id) {
  // Get shelf id for |app_id| and an empty |launch_id|.
  return GetShelfIDForAppIDAndLaunchID(app_id, std::string());
}

ShelfID TestShelfDelegate::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  for (const ShelfItem& item : Shell::Get()->shelf_model()->items()) {
    // Ash's ShelfWindowWatcher handles app panel windows separately.
    if (item.type != TYPE_APP_PANEL && item.app_launch_id.app_id() == app_id &&
        item.app_launch_id.launch_id() == launch_id) {
      return item.id;
    }
  }
  return kInvalidShelfID;
}

const std::string& TestShelfDelegate::GetAppIDForShelfID(ShelfID id) {
  ShelfModel* model = Shell::Get()->shelf_model();
  ash::ShelfItems::const_iterator item = model->ItemByID(id);
  return item != model->items().end() ? item->app_launch_id.app_id()
                                      : base::EmptyString();
}

void TestShelfDelegate::PinAppWithID(const std::string& app_id) {
  SetItemType(GetShelfIDForAppID(app_id), TYPE_PINNED_APP);
  pinned_apps_.insert(app_id);
}

bool TestShelfDelegate::IsAppPinned(const std::string& app_id) {
  return pinned_apps_.find(app_id) != pinned_apps_.end();
}

void TestShelfDelegate::UnpinAppWithID(const std::string& app_id) {
  SetItemType(GetShelfIDForAppID(app_id), TYPE_APP);
  pinned_apps_.erase(app_id);
}

}  // namespace test
}  // namespace ash
