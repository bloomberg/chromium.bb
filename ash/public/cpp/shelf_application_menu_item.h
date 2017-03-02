// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_APPLICATION_MENU_ITEM_H_
#define ASH_PUBLIC_CPP_SHELF_APPLICATION_MENU_ITEM_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"

namespace ash {

// The command id, title, and icon for shelf application menu items.
class ASH_PUBLIC_EXPORT ShelfApplicationMenuItem {
 public:
  // Creates an item with a client-specific |command_id|, a |title|, and an
  // optional |icon| (pass nullptr for no icon).
  ShelfApplicationMenuItem(uint32_t command_id,
                           const base::string16& title,
                           const gfx::Image* icon = nullptr);
  ~ShelfApplicationMenuItem();

  // The title and icon for this menu item.
  uint32_t command_id() const { return command_id_; }
  const base::string16& title() const { return title_; }
  const gfx::Image& icon() const { return icon_; }

 private:
  const uint32_t command_id_;
  const base::string16 title_;
  const gfx::Image icon_;

  DISALLOW_COPY_AND_ASSIGN(ShelfApplicationMenuItem);
};

using ShelfAppMenuItemList =
    std::vector<std::unique_ptr<ShelfApplicationMenuItem>>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_APPLICATION_MENU_ITEM_H_
