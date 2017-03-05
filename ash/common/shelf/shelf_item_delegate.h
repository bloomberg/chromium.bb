// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_
#define ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_application_menu_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/events/event_constants.h"

namespace ash {

// Delegate for the ShelfItem.
class ASH_EXPORT ShelfItemDelegate {
 public:
  ShelfItemDelegate();
  virtual ~ShelfItemDelegate();

  // Called when the user selects a shelf item. The event type and flags, the
  // relevant display id, and the source of the selection should be provided if
  // they are known to the caller; as some subclasses use these arguments.
  // Defaults: (ET_UNKNOWN, EF_NONE, kInvalidDisplayId, LAUNCH_FROM_UNKNOWN)
  // Returns the action performed by selecting the item.
  virtual ShelfAction ItemSelected(ui::EventType event_type,
                                   int event_flags,
                                   int64_t display_id,
                                   ShelfLaunchSource source) = 0;

  // A helper to call ItemSelected with a source and default arguments.
  ShelfAction ItemSelectedBySource(ShelfLaunchSource source);

  // Returns any application menu items that should appear for this shelf item.
  // |event_flags| specifies the flags of the event which triggered this menu.
  virtual ShelfAppMenuItemList GetAppMenuItems(int event_flags) = 0;

  // Called on invocation of a shelf item's application menu command.
  virtual void ExecuteCommand(uint32_t command_id, int event_flags) = 0;

  // Closes all windows associated with this item.
  virtual void Close() = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_ITEM_DELEGATE_H_
