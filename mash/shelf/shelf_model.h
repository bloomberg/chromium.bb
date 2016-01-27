// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_MODEL_H_
#define MASH_SHELF_SHELF_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "mash/shelf/shelf_item_types.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
class ApplicationImpl;
}

namespace mash {
namespace shelf {

class ShelfModelObserver;

// Model used by ShelfView.
class ShelfModel : public mash::wm::mojom::UserWindowObserver {
 public:
  enum Status {
    STATUS_NORMAL,
    // A status that indicates apps are syncing/loading.
    STATUS_LOADING,
  };

  explicit ShelfModel(mojo::ApplicationImpl* app);
  ~ShelfModel() override;

  // Adds a new item to the model. Returns the resulting index.
  int Add(const ShelfItem& item);

  // Adds the item. |index| is the requested insertion index, which may be
  // modified to meet type-based ordering. Returns the actual insertion index.
  int AddAt(int index, const ShelfItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item maintains its existing
  // id and type.
  void Set(int index, const ShelfItem& item);

  // Returns the index of the item by id.
  int ItemIndexByID(ShelfID id) const;

  // Returns the |index| of the item matching |type| in |items_|.
  // Returns -1 if the matching item is not found.
  // Note: Requires a linear search.
  int GetItemIndexForType(ShelfItemType type);

  // Returns the index of the first running application or the index where the
  // first running application would go if there are no running (non pinned)
  // applications yet.
  int FirstRunningAppIndex() const;

  // Returns the index of the first panel or the index where the first panel
  // would go if there are no panels.
  int FirstPanelIndex() const;

  // Returns the id assigned to the next item added.
  ShelfID next_id() const { return next_id_; }

  // Returns a reserved id which will not be used by the |ShelfModel|.
  ShelfID reserve_external_id() { return next_id_++; }

  // Returns an iterator into items() for the item with the specified id, or
  // items().end() if there is no item with the specified id.
  ShelfItems::const_iterator ItemByID(ShelfID id) const;

  const ShelfItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  void SetStatus(Status status);
  Status status() const { return status_; }

  void AddObserver(ShelfModelObserver* observer);
  void RemoveObserver(ShelfModelObserver* observer);

  // TODO(msw): Do not expose this member, used by ShelfView to FocusUserWindow.
  mash::wm::mojom::UserWindowController* user_window_controller() const {
    return user_window_controller_.get();
  }

 private:
   // Overridden from mash::wm::mojom::UserWindowObserver:
   void OnUserWindowObserverAdded(
       mojo::Array<mash::wm::mojom::UserWindowPtr> user_windows) override;
   void OnUserWindowAdded(mash::wm::mojom::UserWindowPtr user_window) override;
   void OnUserWindowRemoved(uint32_t window_id) override;
   void OnUserWindowTitleChanged(uint32_t window_id,
                                 const mojo::String& window_title) override;

  // Makes sure |index| is in line with the type-based order of items. If that
  // is not the case, adjusts index by shifting it to the valid range and
  // returns the new value.
  int ValidateInsertionIndex(ShelfItemType type, int index) const;

  // Return the index of the item by window id.
  int ItemIndexByWindowID(uint32_t window_id) const;

  // ID assigned to the next item.
  ShelfID next_id_;

  ShelfItems items_;
  Status status_;
  base::ObserverList<ShelfModelObserver> observers_;

  mash::wm::mojom::UserWindowControllerPtr user_window_controller_;
  mojo::Binding<mash::wm::mojom::UserWindowObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(ShelfModel);
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_MODEL_H_
