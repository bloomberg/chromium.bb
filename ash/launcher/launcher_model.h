// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_MODEL_H_
#define ASH_LAUNCHER_LAUNCHER_MODEL_H_
#pragma once

#include <vector>

#include "ash/launcher/launcher_types.h"
#include "base/observer_list.h"
#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

class LauncherModelObserver;

// Model used by LauncherView.
class ASH_EXPORT LauncherModel {
 public:
  LauncherModel();
  ~LauncherModel();

  // Adds a new item to the model. Returns the resulting index.
  int Add(const LauncherItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item maintains its existing
  // id and type.
  void Set(int index, const LauncherItem& item);

  // Returns the index of the item by id.
  int ItemIndexByID(int id);

  // Returns the id assigned to the next item added.
  LauncherID next_id() const { return next_id_; }

  // Returns an iterator into items() for the item with the specified id, or
  // items().end() if there is no item with the specified id.
  LauncherItems::const_iterator ItemByID(LauncherID id) const;

  const LauncherItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  void AddObserver(LauncherModelObserver* observer);
  void RemoveObserver(LauncherModelObserver* observer);

 private:
  // Adds the item at the specified index. Returns |index|.
  int AddAt(int index, const LauncherItem& item);

  // Returns the index to add the specified item at.
  int GetIndexToAddItemAt(LauncherItemType type) const;

  // ID assigned to the next item.
  LauncherID next_id_;
  LauncherItems items_;
  ObserverList<LauncherModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LauncherModel);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_MODEL_H_
