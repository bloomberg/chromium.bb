// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shelf_delegate_mus.h"

#include <memory>

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_menu_model.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

namespace ash {

namespace {

// A ShelfItemDelegate used for pinned items.
// TODO(mash): Support open user windows, etc.
class ShelfItemDelegateMus : public ShelfItemDelegate {
 public:
  ShelfItemDelegateMus() {}
  ~ShelfItemDelegateMus() override {}

  void SetDelegate(mojom::ShelfItemDelegateAssociatedPtrInfo delegate) {
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

  mojom::ShelfItemDelegateAssociatedPtr delegate_;
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

}  // namespace

ShelfDelegateMus::ShelfDelegateMus(ShelfModel* model) : model_(model) {}

ShelfDelegateMus::~ShelfDelegateMus() {}

///////////////////////////////////////////////////////////////////////////////
// ShelfDelegate:

void ShelfDelegateMus::OnShelfCreated(WmShelf* shelf) {
  // Notify observers, Chrome will set alignment and auto-hide from prefs.
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs([display_id](mojom::ShelfObserver* observer) {
    observer->OnShelfCreated(display_id);
  });
}

void ShelfDelegateMus::OnShelfDestroyed(WmShelf* shelf) {}

void ShelfDelegateMus::OnShelfAlignmentChanged(WmShelf* shelf) {
  ShelfAlignment alignment = shelf->alignment();
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs(
      [alignment, display_id](mojom::ShelfObserver* observer) {
        observer->OnAlignmentChanged(alignment, display_id);
      });
}

void ShelfDelegateMus::OnShelfAutoHideBehaviorChanged(WmShelf* shelf) {
  ShelfAutoHideBehavior behavior = shelf->auto_hide_behavior();
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs([behavior, display_id](mojom::ShelfObserver* observer) {
    observer->OnAutoHideBehaviorChanged(behavior, display_id);
  });
}

void ShelfDelegateMus::OnShelfAutoHideStateChanged(WmShelf* shelf) {}

void ShelfDelegateMus::OnShelfVisibilityStateChanged(WmShelf* shelf) {}

ShelfID ShelfDelegateMus::GetShelfIDForAppID(const std::string& app_id) {
  if (app_id_to_shelf_id_.count(app_id))
    return app_id_to_shelf_id_[app_id];
  return 0;
}

ShelfID ShelfDelegateMus::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  return ShelfDelegateMus::GetShelfIDForAppID(app_id);
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
// mojom::ShelfController:

void ShelfDelegateMus::AddObserver(
    mojom::ShelfObserverAssociatedPtrInfo observer) {
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  // Notify the observer of the current state.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    WmWindow* root = WmShell::Get()->GetRootWindowForDisplayId(display.id());
    WmShelf* shelf = root->GetRootWindowController()->GetShelf();
    observer_ptr->OnAlignmentChanged(shelf->alignment(), display.id());
    observer_ptr->OnAutoHideBehaviorChanged(shelf->auto_hide_behavior(),
                                            display.id());
  }
  observers_.AddPtr(std::move(observer_ptr));
}

void ShelfDelegateMus::SetAlignment(ShelfAlignment alignment,
                                    int64_t display_id) {
  WmRootWindowController* root_window_controller =
      WmLookup::Get()->GetRootWindowControllerWithDisplayId(display_id);
  // The controller may be null for invalid ids or for displays being removed.
  if (root_window_controller && root_window_controller->HasShelf())
    root_window_controller->GetShelf()->SetAlignment(alignment);
}

void ShelfDelegateMus::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                                           int64_t display_id) {
  WmRootWindowController* root_window_controller =
      WmLookup::Get()->GetRootWindowControllerWithDisplayId(display_id);
  // The controller may be null for invalid ids or for displays being removed.
  if (root_window_controller && root_window_controller->HasShelf())
    root_window_controller->GetShelf()->SetAutoHideBehavior(auto_hide);
}

void ShelfDelegateMus::PinItem(
    mojom::ShelfItemPtr item,
    mojom::ShelfItemDelegateAssociatedPtrInfo delegate) {
  if (app_id_to_shelf_id_.count(item->app_id)) {
    ShelfID shelf_id = app_id_to_shelf_id_[item->app_id];
    ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
    item_delegate->SetDelegate(std::move(delegate));
    item_delegate->set_pinned(true);
    return;
  }

  ShelfID shelf_id = model_->next_id();
  app_id_to_shelf_id_.insert(std::make_pair(item->app_id, shelf_id));
  shelf_id_to_app_id_.insert(std::make_pair(shelf_id, item->app_id));

  ShelfItem shelf_item;
  shelf_item.type = TYPE_APP_SHORTCUT;
  shelf_item.status = STATUS_CLOSED;
  shelf_item.image = GetShelfIconFromBitmap(item->image);
  model_->Add(shelf_item);

  std::unique_ptr<ShelfItemDelegateMus> item_delegate(
      new ShelfItemDelegateMus());
  item_delegate->SetDelegate(std::move(delegate));
  item_delegate->set_pinned(true);
  item_delegate->set_title(base::UTF8ToUTF16(item->app_title));
  model_->SetShelfItemDelegate(shelf_id, std::move(item_delegate));
}

void ShelfDelegateMus::UnpinItem(const std::string& app_id) {
  if (!app_id_to_shelf_id_.count(app_id))
    return;
  ShelfID shelf_id = app_id_to_shelf_id_[app_id];
  ShelfItemDelegateMus* item_delegate = GetShelfItemDelegate(shelf_id);
  DCHECK(item_delegate->pinned());
  item_delegate->set_pinned(false);
  if (item_delegate->window_id_to_title().empty()) {
    model_->RemoveItemAt(model_->ItemIndexByID(shelf_id));
    app_id_to_shelf_id_.erase(app_id);
    shelf_id_to_app_id_.erase(shelf_id);
  }
}

void ShelfDelegateMus::SetItemImage(const std::string& app_id,
                                    const SkBitmap& image) {
  if (!app_id_to_shelf_id_.count(app_id))
    return;
  ShelfID shelf_id = app_id_to_shelf_id_[app_id];
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  ShelfItem item = *model_->ItemByID(shelf_id);
  item.image = GetShelfIconFromBitmap(image);
  model_->Set(index, item);
}

}  // namespace ash
