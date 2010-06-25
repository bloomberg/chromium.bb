// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_MENUS_BUTTON_MENU_ITEM_MODEL_H_
#define APP_MENUS_BUTTON_MENU_ITEM_MODEL_H_

#include <vector>

#include "base/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace menus {

// A model representing the rows of buttons that should be inserted in a button
// containing menu item.
//
// TODO(erg): There are still two major pieces missing from this model. It
// needs to be able to group buttons together so they all have the same
// width. ButtonSides needs to be used to communicate how buttons are squashed
// together.
class ButtonMenuItemModel {
 public:
  // Types of buttons.
  enum ButtonType {
    TYPE_SPACE,
    TYPE_BUTTON
  };

  // Which sides of the button are visible.
  enum ButtonSides {
    SIDE_NONE = 0,
    SIDE_LEFT = 1 << 0,
    SIDE_RIGHT = 1 << 1,
    SIDE_BOTH = SIDE_LEFT | SIDE_RIGHT
  };

  class Delegate {
   public:
    // Some command ids have labels that change over time.
    virtual bool IsLabelForCommandIdDynamic(int command_id) const {
      return false;
    }
    virtual string16 GetLabelForCommandId(int command_id) const {
      return string16();
    }

    // Performs the action associated with the specified command id.
    virtual void ExecuteCommand(int command_id) = 0;
  };

  ButtonMenuItemModel(int string_id, ButtonMenuItemModel::Delegate* delegate);

  // Adds a button that will emit |command_id|.
  void AddItemWithStringId(int command_id, int string_id);

  // Adds a button that has an icon instead of a label.
  void AddItemWithImage(int command_id, int icon_idr);

  // Adds a small horizontal space.
  void AddSpace();

  // Returns the number of items for iteration.
  int GetItemCount() const;

  // Returns what kind of item is at |index|.
  ButtonType GetTypeAt(int index) const;

  // Changes a position into a command ID.
  int GetCommandIdAt(int index) const;

  const string16& GetLabelAt(int index) const;

  // If the button at |index| should have an icon instead, returns true and
  // sets the IDR |icon|.
  bool GetIconAt(int index, int* icon) const;

  // Called from implementations.
  void ActivatedCommand(int command_id);

  const string16& label() const { return item_label_; }

 private:
  // The non-clickable label to the left of the buttons.
  string16 item_label_;

  struct Item {
    int command_id;
    ButtonType type;
    string16 label;
    int sides;
    int icon_idr;
  };
  std::vector<Item> items_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ButtonMenuItemModel);
};

}  // namespace menus

#endif  // APP_MENUS_BUTTON_MENU_ITEM_MODEL_H_
