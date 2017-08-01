// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/remote_shelf_item_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Returns the Shelf instance for the display with the given |display_id|.
Shelf* GetShelfForDisplay(int64_t display_id) {
  // The controller may be null for invalid ids or for displays being removed.
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller ? root_window_controller->shelf() : nullptr;
}

}  // namespace

ShelfController::ShelfController() {
  // Set the delegate and title string for the app list item.
  model_.SetShelfItemDelegate(ShelfID(kAppListId),
                              base::MakeUnique<AppListShelfItemDelegate>());
  DCHECK_EQ(0, model_.ItemIndexByID(ShelfID(kAppListId)));
  ShelfItem item = model_.items()[0];
  item.title = l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE);
  model_.Set(0, item);

  model_.AddObserver(this);
}

ShelfController::~ShelfController() {
  model_.RemoveObserver(this);
}

void ShelfController::BindRequest(mojom::ShelfControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ShelfController::NotifyShelfInitialized(Shelf* shelf) {
  // Notify observers, Chrome will set alignment and auto-hide from prefs.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(shelf->GetWindow());
  int64_t display_id = display.id();
  observers_.ForAllPtrs([display_id](mojom::ShelfObserver* observer) {
    observer->OnShelfInitialized(display_id);
  });
}

void ShelfController::NotifyShelfAlignmentChanged(Shelf* shelf) {
  ShelfAlignment alignment = shelf->alignment();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(shelf->GetWindow());
  int64_t display_id = display.id();
  observers_.ForAllPtrs(
      [alignment, display_id](mojom::ShelfObserver* observer) {
        observer->OnAlignmentChanged(alignment, display_id);
      });
}

void ShelfController::NotifyShelfAutoHideBehaviorChanged(Shelf* shelf) {
  ShelfAutoHideBehavior behavior = shelf->auto_hide_behavior();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(shelf->GetWindow());
  int64_t display_id = display.id();
  observers_.ForAllPtrs([behavior, display_id](mojom::ShelfObserver* observer) {
    observer->OnAutoHideBehaviorChanged(behavior, display_id);
  });
}

void ShelfController::AddObserver(
    mojom::ShelfObserverAssociatedPtrInfo observer) {
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));

  if (Shell::GetAshConfig() == Config::MASH) {
    // Mash synchronizes two ShelfModel instances, owned by Ash and Chrome.
    // Notify Chrome of existing ShelfModel items created by Ash.
    for (int i = 0; i < model_.item_count(); ++i) {
      const ShelfItem& item = model_.items()[i];
      observer_ptr->OnShelfItemAdded(i, item);
      ShelfItemDelegate* delegate = model_.GetShelfItemDelegate(item.id);
      if (delegate) {
        observer_ptr->OnShelfItemDelegateChanged(
            item.id, delegate->CreateInterfacePtrAndBind());
      }
    }
  }

  observers_.AddPtr(std::move(observer_ptr));
}

void ShelfController::SetAlignment(ShelfAlignment alignment,
                                   int64_t display_id) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  // TODO(jamescook): The session state check should not be necessary, but
  // otherwise this wrongly tries to set the alignment on a secondary display
  // during login before the ShelfLockingManager is created.
  if (shelf && Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    shelf->SetAlignment(alignment);
}

void ShelfController::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                                          int64_t display_id) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  // TODO(jamescook): The session state check should not be necessary, but
  // otherwise this wrongly tries to set auto-hide state on a secondary display
  // during login.
  if (shelf && Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    shelf->SetAutoHideBehavior(auto_hide);
}

void ShelfController::AddShelfItem(int32_t index, const ShelfItem& item) {
  DCHECK_EQ(Shell::GetAshConfig(), Config::MASH) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  index = index < 0 ? model_.item_count() : index;
  DCHECK_GT(index, 0) << " Items can not precede the AppList";
  DCHECK_LE(index, model_.item_count()) << " Index out of bounds";
  index = std::min(std::max(index, 1), model_.item_count());
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_.AddAt(index, item);
}

void ShelfController::RemoveShelfItem(const ShelfID& id) {
  DCHECK_EQ(Shell::GetAshConfig(), Config::MASH) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int index = model_.ItemIndexByID(id);
  DCHECK_GE(index, 0) << " No item found with the id: " << id;
  DCHECK_NE(index, 0) << " The AppList shelf item cannot be removed";
  if (index <= 0)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_.RemoveItemAt(index);
}

void ShelfController::MoveShelfItem(const ShelfID& id, int32_t index) {
  DCHECK_EQ(Shell::GetAshConfig(), Config::MASH) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int current_index = model_.ItemIndexByID(id);
  DCHECK_GE(current_index, 0) << " No item found with the id: " << id;
  DCHECK_NE(current_index, 0) << " The AppList shelf item cannot be moved";
  if (current_index <= 0)
    return;
  DCHECK_GT(index, 0) << " Items can not precede the AppList";
  DCHECK_LT(index, model_.item_count()) << " Index out of bounds";
  index = std::min(std::max(index, 1), model_.item_count() - 1);
  DCHECK_NE(current_index, index) << " The item is already at the given index";
  if (current_index == index)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_.Move(current_index, index);
}

void ShelfController::UpdateShelfItem(const ShelfItem& item) {
  DCHECK_EQ(Shell::GetAshConfig(), Config::MASH) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int index = model_.ItemIndexByID(item.id);
  DCHECK_GE(index, 0) << " No item found with the id: " << item.id;
  if (index < 0)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_.Set(index, item);
}

void ShelfController::SetShelfItemDelegate(
    const ShelfID& id,
    mojom::ShelfItemDelegatePtr delegate) {
  DCHECK_EQ(Shell::GetAshConfig(), Config::MASH) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  if (delegate.is_bound())
    model_.SetShelfItemDelegate(
        id, base::MakeUnique<RemoteShelfItemDelegate>(id, std::move(delegate)));
  else
    model_.SetShelfItemDelegate(id, nullptr);
}

void ShelfController::ShelfItemAdded(int index) {
  if (applying_remote_shelf_model_changes_ ||
      Shell::GetAshConfig() != Config::MASH) {
    return;
  }

  const ShelfItem& item = model_.items()[index];
  observers_.ForAllPtrs([index, item](mojom::ShelfObserver* observer) {
    observer->OnShelfItemAdded(index, item);
  });
}

void ShelfController::ShelfItemRemoved(int index, const ShelfItem& old_item) {
  if (applying_remote_shelf_model_changes_ ||
      Shell::GetAshConfig() != Config::MASH) {
    return;
  }

  observers_.ForAllPtrs([old_item](mojom::ShelfObserver* observer) {
    observer->OnShelfItemRemoved(old_item.id);
  });
}

void ShelfController::ShelfItemMoved(int start_index, int target_index) {
  if (applying_remote_shelf_model_changes_ ||
      Shell::GetAshConfig() != Config::MASH) {
    return;
  }

  const ShelfItem& item = model_.items()[target_index];
  observers_.ForAllPtrs([item, target_index](mojom::ShelfObserver* observer) {
    observer->OnShelfItemMoved(item.id, target_index);
  });
}

void ShelfController::ShelfItemChanged(int index, const ShelfItem& old_item) {
  if (applying_remote_shelf_model_changes_ ||
      Shell::GetAshConfig() != Config::MASH) {
    return;
  }

  const ShelfItem& item = model_.items()[index];
  observers_.ForAllPtrs([item](mojom::ShelfObserver* observer) {
    observer->OnShelfItemUpdated(item);
  });
}

void ShelfController::ShelfItemDelegateChanged(const ShelfID& id,
                                               ShelfItemDelegate* delegate) {
  if (applying_remote_shelf_model_changes_ ||
      Shell::GetAshConfig() != Config::MASH) {
    return;
  }

  observers_.ForAllPtrs([id, delegate](mojom::ShelfObserver* observer) {
    observer->OnShelfItemDelegateChanged(
        id, delegate ? delegate->CreateInterfacePtrAndBind()
                     : mojom::ShelfItemDelegatePtr());
  });
}

}  // namespace ash
