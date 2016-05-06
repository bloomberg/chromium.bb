// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shelf_delegate_mus.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_menu_model.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/connector.h"
#include "skia/public/type_converters.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/mus/window_manager_connection.h"

using mash::wm::mojom::UserWindowController;

namespace ash {
namespace sysui {

namespace {

// A ShelfItemDelegate used for pinned items and open user windows.
class ShelfItemDelegateMus : public ShelfItemDelegate {
 public:
  explicit ShelfItemDelegateMus(UserWindowController* user_window_controller)
      : user_window_controller_(user_window_controller) {}
  ~ShelfItemDelegateMus() override {}

  void SetDelegate(
      mash::shelf::mojom::ShelfItemDelegateAssociatedPtrInfo delegate) {
    delegate_.Bind(std::move(delegate));
  }

  bool pinned() const { return pinned_; }
  void set_pinned(bool pinned) { pinned_ = pinned; }

  void AddWindow(uint32_t id, const base::string16& title) {
    DCHECK(!window_id_to_title_.count(id));
    window_id_to_title_.insert(std::make_pair(id, title));
  }
  void RemoveWindow(uint32_t id) { window_id_to_title_.erase(id); }
  void SetWindowTitle(uint32_t id, const base::string16& title) {
    DCHECK(window_id_to_title_.count(id));
    window_id_to_title_[id] = title;
  }
  const std::map<uint32_t, base::string16>& window_id_to_title() const {
    return window_id_to_title_;
  }

  const base::string16& title() { return title_; }
  void set_title(const base::string16& title) { title_ = title; }

 private:
  // This application menu model for ShelfItemDelegateMus lists open windows.
  class ShelfMenuModelMus : public ShelfMenuModel,
                            public ui::SimpleMenuModel::Delegate {
   public:
    explicit ShelfMenuModelMus(ShelfItemDelegateMus* item_delegate)
        : ShelfMenuModel(this), item_delegate_(item_delegate) {
      AddSeparator(ui::SPACING_SEPARATOR);
      AddItem(0, item_delegate_->GetTitle());
      AddSeparator(ui::SPACING_SEPARATOR);
      for (const auto& window : item_delegate_->window_id_to_title())
        AddItem(window.first, window.second);
      AddSeparator(ui::SPACING_SEPARATOR);
    }
    ~ShelfMenuModelMus() override {}

    // ShelfMenuModel:
    bool IsCommandActive(int command_id) const override { return false; }

    // ui::SimpleMenuModel::Delegate:
    bool IsCommandIdChecked(int command_id) const override { return false; }
    bool IsCommandIdEnabled(int command_id) const override {
      return command_id > 0;
    }
    bool GetAcceleratorForCommandId(int command_id,
                                    ui::Accelerator* accelerator) override {
      return false;
    }
    void ExecuteCommand(int command_id, int event_flags) override {
      item_delegate_->user_window_controller_->FocusUserWindow(command_id);
    }

   private:
    ShelfItemDelegateMus* item_delegate_;

    DISALLOW_COPY_AND_ASSIGN(ShelfMenuModelMus);
  };

  // ShelfItemDelegate:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override {
    if (window_id_to_title_.empty()) {
      delegate_->LaunchItem();
      return kNewWindowCreated;
    }
    if (window_id_to_title_.size() == 1) {
      user_window_controller_->FocusUserWindow(
          window_id_to_title_.begin()->first);
      return kExistingWindowActivated;
    }
    return kNoAction;
  }

  base::string16 GetTitle() override {
    return window_id_to_title_.empty() ? title_
                                       : window_id_to_title_.begin()->second;
  }

  bool CanPin() const override {
    NOTIMPLEMENTED();
    return true;
  }

  ShelfMenuModel* CreateApplicationMenu(int event_flags) override {
    return new ShelfMenuModelMus(this);
  }

  bool IsDraggable() override {
    NOTIMPLEMENTED();
    return false;
  }

  bool ShouldShowTooltip() override { return true; }

  void Close() override { NOTIMPLEMENTED(); }

  mash::shelf::mojom::ShelfItemDelegateAssociatedPtr delegate_;
  bool pinned_ = false;
  std::map<uint32_t, base::string16> window_id_to_title_;
  base::string16 title_;
  UserWindowController* user_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegateMus);
};

ShelfItemDelegateMus* GetShelfItemDelegate(ShelfID shelf_id) {
  return static_cast<ShelfItemDelegateMus*>(
      Shell::GetInstance()->shelf_item_delegate_manager()->GetShelfItemDelegate(
          shelf_id));
}

// Returns an icon image from an SkBitmap, or the default shelf icon
// image if the bitmap is empty. Assumes the bitmap is a 1x icon.
// TODO(jamescook): Support other scale factors.
gfx::ImageSkia GetShelfIconFromBitmap(const SkBitmap& bitmap) {
  gfx::ImageSkia icon_image;
  if (!bitmap.isNull()) {
    icon_image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  } else {
    // Use default icon.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    icon_image = *rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
  }
  return icon_image;
}

// Returns an icon image from a serialized SkBitmap.
gfx::ImageSkia GetShelfIconFromSerializedBitmap(
    const mojo::Array<uint8_t>& serialized_bitmap) {
  SkBitmap bitmap = mojo::ConvertTo<SkBitmap>(serialized_bitmap.storage());
  return GetShelfIconFromBitmap(bitmap);
}

}  // namespace

ShelfDelegateMus::ShelfDelegateMus(ShelfModel* model)
    : model_(model), binding_(this) {
  ::shell::Connector* connector =
      views::WindowManagerConnection::Get()->connector();
  connector->ConnectToInterface("mojo:desktop_wm", &shelf_layout_);
  connector->ConnectToInterface("mojo:desktop_wm", &user_window_controller_);
  user_window_controller_->AddUserWindowObserver(
      binding_.CreateInterfacePtrAndBind());
}

ShelfDelegateMus::~ShelfDelegateMus() {}

void ShelfDelegateMus::OnShelfCreated(Shelf* shelf) {
  SetShelfPreferredSizes(shelf);
}

void ShelfDelegateMus::OnShelfDestroyed(Shelf* shelf) {
  NOTIMPLEMENTED();
}

void ShelfDelegateMus::OnShelfAlignmentChanged(Shelf* shelf) {
  SetShelfPreferredSizes(shelf);
  mash::shelf::mojom::Alignment alignment =
      static_cast<mash::shelf::mojom::Alignment>(shelf->alignment());
  shelf_layout_->SetAlignment(alignment);

  observers_.ForAllPtrs(
      [alignment](mash::shelf::mojom::ShelfObserver* observer) {
        observer->OnAlignmentChanged(alignment);
      });
}

void ShelfDelegateMus::OnShelfAutoHideBehaviorChanged(Shelf* shelf) {
  mash::shelf::mojom::AutoHideBehavior behavior =
      static_cast<mash::shelf::mojom::AutoHideBehavior>(
          shelf->auto_hide_behavior());
  shelf_layout_->SetAutoHideBehavior(behavior);

  observers_.ForAllPtrs(
      [behavior](mash::shelf::mojom::ShelfObserver* observer) {
        observer->OnAutoHideBehaviorChanged(behavior);
      });
}

ShelfID ShelfDelegateMus::GetShelfIDForAppID(const std::string& app_id) {
  if (app_id_to_shelf_id_.count(app_id))
    return app_id_to_shelf_id_[app_id];
  return 0;
}

bool ShelfDelegateMus::HasShelfIDToAppIDMapping(ShelfID id) const {
  return shelf_id_to_app_id_.count(id) != 0;
}

const std::string& ShelfDelegateMus::GetAppIDForShelfID(ShelfID id) {
  if (shelf_id_to_app_id_.count(id))
    return shelf_id_to_app_id_[id];
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

void ShelfDelegateMus::AddObserver(
    mash::shelf::mojom::ShelfObserverAssociatedPtrInfo observer) {
  mash::shelf::mojom::ShelfObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  // Notify the observer of the current state.
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  observer_ptr->OnAlignmentChanged(
      static_cast<mash::shelf::mojom::Alignment>(shelf->alignment()));
  observer_ptr->OnAutoHideBehaviorChanged(
      static_cast<mash::shelf::mojom::AutoHideBehavior>(
          shelf->auto_hide_behavior()));
  observers_.AddPtr(std::move(observer_ptr));
}

void ShelfDelegateMus::SetAlignment(mash::shelf::mojom::Alignment alignment) {
  wm::ShelfAlignment value = static_cast<wm::ShelfAlignment>(alignment);
  Shell::GetInstance()->SetShelfAlignment(value, Shell::GetPrimaryRootWindow());
}

void ShelfDelegateMus::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  ShelfAutoHideBehavior value = static_cast<ShelfAutoHideBehavior>(auto_hide);
  Shell::GetInstance()->SetShelfAutoHideBehavior(value,
                                                 Shell::GetPrimaryRootWindow());
}

void ShelfDelegateMus::PinItem(
    mash::shelf::mojom::ShelfItemPtr item,
    mash::shelf::mojom::ShelfItemDelegateAssociatedPtrInfo delegate) {
  std::string app_id(item->app_id.To<std::string>());
  if (app_id_to_shelf_id_.count(app_id)) {
    ShelfID shelf_id = app_id_to_shelf_id_[app_id];
    ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
    item_delegate->SetDelegate(std::move(delegate));
    item_delegate->set_pinned(true);
    return;
  }

  ShelfID shelf_id = model_->next_id();
  app_id_to_shelf_id_.insert(std::make_pair(app_id, shelf_id));
  shelf_id_to_app_id_.insert(std::make_pair(shelf_id, app_id));

  ShelfItem shelf_item;
  shelf_item.type = TYPE_APP_SHORTCUT;
  shelf_item.status = STATUS_CLOSED;
  shelf_item.image = GetShelfIconFromBitmap(item->image.To<SkBitmap>());
  model_->Add(shelf_item);

  std::unique_ptr<ShelfItemDelegateMus> item_delegate(
      new ShelfItemDelegateMus(user_window_controller_.get()));
  item_delegate->SetDelegate(std::move(delegate));
  item_delegate->set_pinned(true);
  item_delegate->set_title(item->app_title.To<base::string16>());
  Shell::GetInstance()->shelf_item_delegate_manager()->SetShelfItemDelegate(
      shelf_id, std::move(item_delegate));
}

void ShelfDelegateMus::UnpinItem(const mojo::String& app_id) {
  if (!app_id_to_shelf_id_.count(app_id.To<std::string>()))
    return;
  ShelfID shelf_id = app_id_to_shelf_id_[app_id.To<std::string>()];
  ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
  DCHECK(item_delegate->pinned());
  item_delegate->set_pinned(false);
  if (item_delegate->window_id_to_title().empty()) {
    model_->RemoveItemAt(model_->ItemIndexByID(shelf_id));
    app_id_to_shelf_id_.erase(app_id.To<std::string>());
    shelf_id_to_app_id_.erase(shelf_id);
  }
}

void ShelfDelegateMus::SetItemImage(const mojo::String& app_id,
                                    skia::mojom::BitmapPtr image) {
  if (!app_id_to_shelf_id_.count(app_id.To<std::string>()))
    return;
  ShelfID shelf_id = app_id_to_shelf_id_[app_id.To<std::string>()];
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItem item = *model_->ItemByID(shelf_id);
  item.image = GetShelfIconFromBitmap(image.To<SkBitmap>());
  model_->Set(index, item);
}

void ShelfDelegateMus::OnUserWindowObserverAdded(
    mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) {
  for (size_t i = 0; i < user_windows.size(); ++i)
    OnUserWindowAdded(std::move(user_windows[i]));
}

void ShelfDelegateMus::OnUserWindowAdded(
    mash::wm::mojom::UserWindowPtr user_window) {
  DCHECK(!window_id_to_shelf_id_.count(user_window->window_id));

  if (user_window->ignored_by_shelf)
    return;

  std::string app_id(user_window->window_app_id.To<std::string>());
  if (app_id_to_shelf_id_.count(app_id)) {
    ShelfID shelf_id = app_id_to_shelf_id_[app_id];
    window_id_to_shelf_id_.insert(
        std::make_pair(user_window->window_id, shelf_id));

    ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
    item_delegate->AddWindow(user_window->window_id,
                             user_window->window_title.To<base::string16>());
    return;
  }

  ShelfID shelf_id = model_->next_id();
  window_id_to_shelf_id_.insert(
      std::make_pair(user_window->window_id, shelf_id));
  app_id_to_shelf_id_.insert(std::make_pair(app_id, shelf_id));
  shelf_id_to_app_id_.insert(std::make_pair(shelf_id, app_id));

  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = user_window->window_has_focus ? STATUS_ACTIVE : STATUS_RUNNING;
  item.image = GetShelfIconFromSerializedBitmap(user_window->window_app_icon);
  model_->Add(item);

  std::unique_ptr<ShelfItemDelegateMus> item_delegate(
      new ShelfItemDelegateMus(user_window_controller_.get()));
  item_delegate->AddWindow(user_window->window_id,
                           user_window->window_title.To<base::string16>());
  Shell::GetInstance()->shelf_item_delegate_manager()->SetShelfItemDelegate(
      shelf_id, std::move(item_delegate));
}

void ShelfDelegateMus::OnUserWindowRemoved(uint32_t window_id) {
  if (!window_id_to_shelf_id_.count(window_id))
    return;
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
  item_delegate->RemoveWindow(window_id);
  window_id_to_shelf_id_.erase(window_id);
  if (item_delegate->window_id_to_title().empty() && !item_delegate->pinned()) {
    model_->RemoveItemAt(model_->ItemIndexByID(shelf_id));
    const std::string& app_id = shelf_id_to_app_id_[shelf_id];
    app_id_to_shelf_id_.erase(app_id);
    shelf_id_to_app_id_.erase(shelf_id);
  }
}

void ShelfDelegateMus::OnUserWindowTitleChanged(
    uint32_t window_id,
    const mojo::String& window_title) {
  if (!window_id_to_shelf_id_.count(window_id))
    return;
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
  item_delegate->SetWindowTitle(window_id, window_title.To<base::string16>());

  // There's nothing in the ShelfItem that needs to be updated. But we still
  // need to update the ShelfModel so that the observers can pick up any
  // changes.
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItems::const_iterator iter = model_->ItemByID(shelf_id);
  DCHECK(iter != model_->items().end());
  model_->Set(index, *iter);
}

void ShelfDelegateMus::OnUserWindowAppIconChanged(
    uint32_t window_id,
    mojo::Array<uint8_t> app_icon) {
  if (!window_id_to_shelf_id_.count(window_id))
    return;
  // Find the shelf ID for this window.
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  DCHECK_GT(shelf_id, 0);

  // Update the icon in the ShelfItem.
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItem item = *model_->ItemByID(shelf_id);
  item.image = GetShelfIconFromSerializedBitmap(app_icon);
  model_->Set(index, item);
}

void ShelfDelegateMus::OnUserWindowFocusChanged(uint32_t window_id,
                                                bool has_focus) {
  if (!window_id_to_shelf_id_.count(window_id))
    return;
  ShelfID shelf_id = window_id_to_shelf_id_[window_id];
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItems::const_iterator iter = model_->ItemByID(shelf_id);
  DCHECK(iter != model_->items().end());
  ShelfItem item = *iter;
  item.status = has_focus ? STATUS_ACTIVE : STATUS_RUNNING;
  model_->Set(index, item);
}

void ShelfDelegateMus::SetShelfPreferredSizes(Shelf* shelf) {
  ShelfWidget* widget = shelf->shelf_widget();
  ShelfLayoutManager* layout_manager = widget->shelf_layout_manager();
  mus::Window* window = aura::GetMusWindow(widget->GetNativeWindow());
  gfx::Size size = layout_manager->GetIdealBounds().size();
  window->SetSharedProperty<gfx::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, size);

  StatusAreaWidget* status_widget = widget->status_area_widget();
  mus::Window* status_window =
      aura::GetMusWindow(status_widget->GetNativeWindow());
  gfx::Size status_size = status_widget->GetWindowBoundsInScreen().size();
  status_window->SetSharedProperty<gfx::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, status_size);
}

}  // namespace sysui
}  // namespace ash
