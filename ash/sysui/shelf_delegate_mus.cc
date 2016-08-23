// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/shelf_delegate_mus.h"

#include <memory>

#include "ash/common/shelf/shelf.h"
#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_menu_model.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/wm_shell.h"
#include "base/strings/string_util.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/connector.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/mus/window_manager_connection.h"

namespace ash {
namespace sysui {

namespace {

// A ShelfItemDelegate used for pinned items.
// TODO(mash): Support open user windows, etc.
class ShelfItemDelegateMus : public ShelfItemDelegate {
 public:
  ShelfItemDelegateMus() {}
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
    void ExecuteCommand(int command_id, int event_flags) override {
      NOTIMPLEMENTED();
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
      // TODO(mash): Activate the window and return kExistingWindowActivated.
      NOTIMPLEMENTED();
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

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegateMus);
};

ShelfItemDelegateMus* GetShelfItemDelegate(ShelfID shelf_id) {
  return static_cast<ShelfItemDelegateMus*>(
      WmShell::Get()->shelf_model()->GetShelfItemDelegate(shelf_id));
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

ShelfDelegateMus::ShelfDelegateMus(ShelfModel* model) : model_(model) {}

ShelfDelegateMus::~ShelfDelegateMus() {}

///////////////////////////////////////////////////////////////////////////////
// ShelfDelegate:

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
  observers_.ForAllPtrs(
      [alignment](mash::shelf::mojom::ShelfObserver* observer) {
        observer->OnAlignmentChanged(alignment);
      });
}

void ShelfDelegateMus::OnShelfAutoHideBehaviorChanged(Shelf* shelf) {
  mash::shelf::mojom::AutoHideBehavior behavior =
      static_cast<mash::shelf::mojom::AutoHideBehavior>(
          shelf->auto_hide_behavior());
  observers_.ForAllPtrs(
      [behavior](mash::shelf::mojom::ShelfObserver* observer) {
        observer->OnAutoHideBehaviorChanged(behavior);
      });
}

void ShelfDelegateMus::OnShelfAutoHideStateChanged(Shelf* shelf) {
  // Push the new preferred size to the window manager. For example, when the
  // shelf is auto-hidden it becomes a very short "light bar".
  SetShelfPreferredSizes(shelf);
}

void ShelfDelegateMus::OnShelfVisibilityStateChanged(Shelf* shelf) {
  SetShelfPreferredSizes(shelf);
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

///////////////////////////////////////////////////////////////////////////////
// mash::shelf::mojom::ShelfController:

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
  ShelfAlignment value = static_cast<ShelfAlignment>(alignment);
  Shelf::ForPrimaryDisplay()->SetAlignment(value);
}

void ShelfDelegateMus::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  ShelfAutoHideBehavior value = static_cast<ShelfAutoHideBehavior>(auto_hide);
  Shelf::ForPrimaryDisplay()->SetAutoHideBehavior(value);
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
  shelf_item.image = GetShelfIconFromBitmap(item->image);
  model_->Add(shelf_item);

  std::unique_ptr<ShelfItemDelegateMus> item_delegate(
      new ShelfItemDelegateMus());
  item_delegate->SetDelegate(std::move(delegate));
  item_delegate->set_pinned(true);
  item_delegate->set_title(item->app_title.To<base::string16>());
  model_->SetShelfItemDelegate(shelf_id, std::move(item_delegate));
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
                                    const SkBitmap& image) {
  if (!app_id_to_shelf_id_.count(app_id.To<std::string>()))
    return;
  ShelfID shelf_id = app_id_to_shelf_id_[app_id.To<std::string>()];
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItem item = *model_->ItemByID(shelf_id);
  item.image = GetShelfIconFromBitmap(image);
  model_->Set(index, item);
}

void ShelfDelegateMus::SetShelfPreferredSizes(Shelf* shelf) {
  ShelfWidget* widget = shelf->shelf_widget();
  ShelfLayoutManager* layout_manager = widget->shelf_layout_manager();
  ui::Window* window = aura::GetMusWindow(widget->GetNativeWindow());
  gfx::Size size = layout_manager->GetPreferredSize();
  window->SetSharedProperty<gfx::Size>(
      ui::mojom::WindowManager::kPreferredSize_Property, size);

  StatusAreaWidget* status_widget = widget->status_area_widget();
  ui::Window* status_window =
      aura::GetMusWindow(status_widget->GetNativeWindow());
  gfx::Size status_size = status_widget->GetWindowBoundsInScreen().size();
  status_window->SetSharedProperty<gfx::Size>(
      ui::mojom::WindowManager::kPreferredSize_Property, status_size);
}

}  // namespace sysui
}  // namespace ash
