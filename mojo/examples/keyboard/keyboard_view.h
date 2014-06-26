// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_KEYBOARD_KEYBOARD_VIEW_H_
#define MOJO_EXAMPLES_KEYBOARD_KEYBOARD_VIEW_H_

#include <vector>

#include "ui/gfx/font_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace mojo {
namespace examples {

class KeyboardDelegate;
struct Key;
struct Row;

// Shows a keyboard the user can interact with. The delegate is notified any
// time the user presses a button.
class KeyboardView : public views::View, public views::ButtonListener {
 public:
  explicit KeyboardView(KeyboardDelegate* delegate);
  virtual ~KeyboardView();

  // views::View:
  virtual void Layout() OVERRIDE;

 private:
  // The type of keys that are shown.
  enum KeyboardLayout {
    KEYBOARD_LAYOUT_ALPHA,

    // Uppercase characters.
    KEYBOARD_LAYOUT_SHIFT,

    // Numeric characters.
    KEYBOARD_LAYOUT_NUMERIC,
  };

  int event_flags() const {
    return (keyboard_layout_ == KEYBOARD_LAYOUT_SHIFT) ?
      ui::EF_SHIFT_DOWN : ui::EF_NONE;
  }

  void SetLayout(KeyboardLayout layout);

  // Lays out the buttons for the specified row.
  void LayoutRow(const Row& row,
                 int row_index,
                 int initial_x,
                 int button_width,
                 int button_height);

  // Sets the rows to show.
  void SetRows(const std::vector<const Row*>& rows);

  // Configures the button in a row.
  void ConfigureButtonsInRow(int row_index, const Row& row);

  // Creates a new button.
  views::View* CreateButton();

  // Returns the button corresponding to a key at the specified row/column.
  views::LabelButton* GetButton(int row, int column);

  const Key& GetKeyForButton(views::Button* button) const;

  // Reset the fonts of all the buttons. |special_font| is used for the buttons
  // that toggle the layout.
  void ResetFonts(const gfx::FontList& button_font,
                  const gfx::FontList& special_font);

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  KeyboardDelegate* delegate_;

  // Maximium number of keys in a row. Determined from |rows_|.
  int max_keys_in_row_;

  KeyboardLayout keyboard_layout_;

  std::vector<const Row*> rows_;

  gfx::Size last_layout_size_;

  gfx::FontList button_font_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardView);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_KEYBOARD_KEYBOARD_VIEW_H_
