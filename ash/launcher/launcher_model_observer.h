// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_MODEL_OBSERVER_H_
#define ASH_LAUNCHER_LAUNCHER_MODEL_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"

namespace ash {

struct LauncherItem;

class ASH_EXPORT LauncherModelObserver {
 public:
  // Invoked after an item has been added to the model.
  virtual void LauncherItemAdded(int index) = 0;

  // Invoked after an item has been removed. |index| is the index the item was
  // at.
  virtual void LauncherItemRemoved(int index, LauncherID id) = 0;

  // Invoked after an item has been moved. See LauncherModel::Move() for details
  // of the arguments.
  virtual void LauncherItemMoved(int start_index, int target_index) = 0;

  // Invoked when the the state of an item changes. |old_item| is the item
  // before the change.
  virtual void LauncherItemChanged(int index, const LauncherItem& old_item) = 0;

 protected:
  virtual ~LauncherModelObserver() {}
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_MODEL_OBSERVER_H_
