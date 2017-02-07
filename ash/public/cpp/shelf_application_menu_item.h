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

// The title, icon, and execute function for shelf application menu items.
class ASH_PUBLIC_EXPORT ShelfApplicationMenuItem {
 public:
  // Creates an item with a |title| and optional |icon| (pass nullptr for none).
  explicit ShelfApplicationMenuItem(const base::string16 title,
                                    const gfx::Image* icon = nullptr);
  virtual ~ShelfApplicationMenuItem();

  // The title and icon for this menu item.
  const base::string16& title() const { return title_; }
  const gfx::Image& icon() const { return icon_; }

  // Executes the menu item; |event_flags| can be used to check additional
  // keyboard modifiers from the event that issued this command.
  virtual void Execute(int event_flags);

 private:
  const base::string16 title_;
  const gfx::Image icon_;

  DISALLOW_COPY_AND_ASSIGN(ShelfApplicationMenuItem);
};

using ShelfAppMenuItemList =
    std::vector<std::unique_ptr<ShelfApplicationMenuItem>>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_APPLICATION_MENU_ITEM_H_
