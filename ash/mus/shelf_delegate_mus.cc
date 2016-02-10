// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shelf_delegate_mus.h"

#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/shell/public/cpp/shell.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/mus/window_manager_connection.h"

using mash::wm::mojom::UserWindowController;

namespace ash {
namespace sysui {

namespace {

class ShelfItemDelegateMus : public ShelfItemDelegate {
 public:
  ShelfItemDelegateMus(uint32_t window_id,
                       const base::string16& title,
                       UserWindowController* user_window_controller)
      : window_id_(window_id),
        title_(title),
        user_window_controller_(user_window_controller) {}
  ~ShelfItemDelegateMus() override {}

  void UpdateTitle(const base::string16& new_title) { title_ = new_title; }

 private:
  // ShelfItemDelegate:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override {
    user_window_controller_->FocusUserWindow(window_id_);
    return kExistingWindowActivated;
  }

  base::string16 GetTitle() override { return title_; }

  ui::MenuModel* CreateContextMenu(aura::Window* root_window) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool CanPin() const override {
    NOTIMPLEMENTED();
    return false;
  }

  ShelfMenuModel* CreateApplicationMenu(int event_flags) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool IsDraggable() override {
    NOTIMPLEMENTED();
    return false;
  }

  bool ShouldShowTooltip() override {
    // NOTIMPLEMENTED(); very noisy
    return false;
  }

  void Close() override { NOTIMPLEMENTED(); }

  // TODO(msw): Support multiple open windows per button.
  uint32_t window_id_;
  base::string16 title_;
  UserWindowController* user_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegateMus);
};

}  // namespace

ShelfDelegateMus::ShelfDelegateMus(ShelfModel* model)
    : model_(model), binding_(this) {
  mojo::Shell* shell = views::WindowManagerConnection::Get()->shell();
  shell->ConnectToInterface("mojo:desktop_wm", &user_window_controller_);
  user_window_controller_->AddUserWindowObserver(
      binding_.CreateInterfacePtrAndBind());
}

ShelfDelegateMus::~ShelfDelegateMus() {}

void ShelfDelegateMus::OnShelfCreated(Shelf* shelf) {
  NOTIMPLEMENTED();
}

void ShelfDelegateMus::OnShelfDestroyed(Shelf* shelf) {
  NOTIMPLEMENTED();
}

ShelfID ShelfDelegateMus::GetShelfIDForAppID(const std::string& app_id) {
  NOTIMPLEMENTED();
  return 0;
}

bool ShelfDelegateMus::HasShelfIDToAppIDMapping(ShelfID id) const {
  NOTIMPLEMENTED();
  return false;
}

const std::string& ShelfDelegateMus::GetAppIDForShelfID(ShelfID id) {
  NOTIMPLEMENTED();
  return base::EmptyString();
}

void ShelfDelegateMus::PinAppWithID(const std::string& app_id) {
  NOTIMPLEMENTED();
}

bool ShelfDelegateMus::IsAppPinned(const std::string& app_id) {
  NOTIMPLEMENTED();
  return false;
}

void ShelfDelegateMus::UnpinAppWithID(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ShelfDelegateMus::OnUserWindowObserverAdded(
    mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) {
  for (size_t i = 0; i < user_windows.size(); ++i)
    OnUserWindowAdded(std::move(user_windows[i]));
}

void ShelfDelegateMus::OnUserWindowAdded(
    mash::wm::mojom::UserWindowPtr user_window) {
  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = user_window->window_has_focus ? STATUS_ACTIVE : STATUS_RUNNING;
  // TODO(msw): Support actual window icons.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  item.image = *rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);

  ShelfID shelf_id = model_->next_id();
  window_id_to_shelf_id_.insert(
      std::make_pair(user_window->window_id, shelf_id));
  model_->Add(item);

  ShelfItemDelegateManager* manager =
      Shell::GetInstance()->shelf_item_delegate_manager();
  scoped_ptr<ShelfItemDelegate> delegate(new ShelfItemDelegateMus(
      user_window->window_id, user_window->window_title.To<base::string16>(),
      user_window_controller_.get()));
  manager->SetShelfItemDelegate(shelf_id, std::move(delegate));
}

void ShelfDelegateMus::OnUserWindowRemoved(uint32_t window_id) {
  DCHECK(window_id_to_shelf_id_.count(window_id));
  model_->RemoveItemAt(
      model_->ItemIndexByID(window_id_to_shelf_id_[window_id]));
}

void ShelfDelegateMus::OnUserWindowTitleChanged(
    uint32_t window_id,
    const mojo::String& window_title) {
  DCHECK(window_id_to_shelf_id_.count(window_id));
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  ShelfItemDelegateManager* manager =
      Shell::GetInstance()->shelf_item_delegate_manager();
  ShelfItemDelegate* delegate = manager->GetShelfItemDelegate(shelf_id);
  DCHECK(delegate);
  static_cast<ShelfItemDelegateMus*>(delegate)->UpdateTitle(
      window_title.To<base::string16>());

  // There's nothing in the ShelfItem that needs to be updated. But we still
  // need to update the ShelfModel so that the observers can pick up any
  // changes.
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItems::const_iterator iter = model_->ItemByID(shelf_id);
  DCHECK(iter != model_->items().end());
  model_->Set(index, *iter);
}

void ShelfDelegateMus::OnUserWindowFocusChanged(uint32_t window_id,
                                                bool has_focus) {
  DCHECK(window_id_to_shelf_id_.count(window_id));
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItems::const_iterator iter = model_->ItemByID(shelf_id);
  DCHECK(iter != model_->items().end());
  ShelfItem item = *iter;
  item.status = has_focus ? STATUS_ACTIVE : STATUS_RUNNING;
  model_->Set(index, item);
}

}  // namespace sysui
}  // namespace ash
