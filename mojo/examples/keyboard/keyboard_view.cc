// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/keyboard/keyboard_view.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/examples/keyboard/keyboard_delegate.h"
#include "mojo/examples/keyboard/keys.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

namespace mojo {
namespace examples {

namespace {

const int kHorizontalPadding = 6;
const int kVerticalPadding = 8;

base::string16 GetDisplayString(int key_code, int flags) {
  return base::string16(1, ui::GetCharacterFromKeyCode(
                            static_cast<ui::KeyboardCode>(key_code), flags));
}

// Returns a font that fits in the space provided. |text| is used as a basis
// for determing the size.
gfx::FontList CalculateFont(int width, int height, const base::string16& text) {
  gfx::FontList font;
  gfx::FontList last_font;
  while (gfx::Canvas::GetStringWidth(text, font) < width &&
         font.GetHeight() < height) {
    last_font = font;
    font = font.DeriveWithSizeDelta(2);
  }
  return last_font;
}

// Returns the total number of keys in |rows|.
int NumKeys(const std::vector<const Row*>& rows) {
  int result = 0;
  for (size_t i = 0; i < rows.size(); ++i)
    result += static_cast<int>(rows[i]->num_keys);
  return result;
}

}  // namespace

KeyboardView::KeyboardView(KeyboardDelegate* delegate)
    : delegate_(delegate),
      max_keys_in_row_(0),
      keyboard_layout_(KEYBOARD_LAYOUT_ALPHA) {
  set_background(views::Background::CreateSolidBackground(SK_ColorBLACK));
  SetRows(GetQWERTYRows());
}

KeyboardView::~KeyboardView() {
}

void KeyboardView::Layout() {
  if (width() == 0 || height() == 0 || rows_.empty() ||
      last_layout_size_ == bounds().size())
    return;

  last_layout_size_ = bounds().size();

  const int button_width =
      (width() - (max_keys_in_row_ - 1) * kHorizontalPadding) /
      max_keys_in_row_;
  const int button_height =
      (height() - (static_cast<int>(rows_.size() - 1) * kVerticalPadding)) /
      static_cast<int>(rows_.size());
  const int initial_x = (width() - button_width * max_keys_in_row_ -
                         kHorizontalPadding * (max_keys_in_row_ - 1)) / 2;
  for (size_t i = 0; i < rows_.size(); ++i) {
    LayoutRow(*(rows_[i]), static_cast<int>(i), initial_x, button_width,
              button_height);
  }

  views::LabelButtonBorder border(views::Button::STYLE_TEXTBUTTON);
  gfx::Insets insets(border.GetInsets());
  gfx::FontList font = CalculateFont(button_width - insets.width(),
                                     button_height - insets.height(),
                                     base::ASCIIToUTF16("W"));
  gfx::FontList special_font = CalculateFont(button_width - insets.width(),
                                             button_height - insets.height(),
                                             base::ASCIIToUTF16("?123"));
  button_font_ = font;
  ResetFonts(font, special_font);
}

void KeyboardView::SetLayout(KeyboardLayout keyboard_layout) {
  if (keyboard_layout_ == keyboard_layout)
    return;

  keyboard_layout_ = keyboard_layout;
  last_layout_size_ = gfx::Size();
  if (keyboard_layout_ == KEYBOARD_LAYOUT_NUMERIC)
    SetRows(GetNumericRows());
  else
    SetRows(GetQWERTYRows());
  Layout();
  SchedulePaint();
}

void KeyboardView::LayoutRow(const Row& row,
                             int row_index,
                             int initial_x,
                             int button_width,
                             int button_height) {
  int x = initial_x + row.padding * (button_width + kHorizontalPadding);
  const int y = row_index * (button_height + kVerticalPadding);
  for (size_t i = 0; i < row.num_keys; ++i) {
    views::View* button = GetButton(row_index, static_cast<int>(i));
    int actual_width = button_width;
    if (row.keys[i].size > 1) {
      actual_width = (button_width + kHorizontalPadding) *
          row.keys[i].size - kHorizontalPadding;
    }
    button->SetBounds(x, y, actual_width, button_height);
    x += actual_width + kHorizontalPadding;
  }
}

void KeyboardView::SetRows(const std::vector<const Row*>& rows) {
  const int num_keys = NumKeys(rows);
  while (child_count() > num_keys)
    delete child_at(child_count() - 1);
  for (int i = child_count(); i < num_keys; ++i)
    AddChildView(CreateButton());

  last_layout_size_ = gfx::Size();

  rows_ = rows;

  max_keys_in_row_ = 0;
  for (size_t i = 0; i < rows_.size(); ++i) {
    max_keys_in_row_ = std::max(max_keys_in_row_,
                                static_cast<int>(rows_[i]->num_keys));
    ConfigureButtonsInRow(static_cast<int>(i), *rows_[i]);
  }
}

void KeyboardView::ConfigureButtonsInRow(int row_index, const Row& row) {
  for (size_t i = 0; i < row.num_keys; ++i) {
    views::LabelButton* button = GetButton(row_index, static_cast<int>(i));
    const Key& key(row.keys[i]);
    switch (key.display_code) {
      case SPECIAL_KEY_SHIFT:
        // TODO: need image.
        button->SetText(base::string16());
        break;
      case SPECIAL_KEY_NUMERIC:
        button->SetText(base::ASCIIToUTF16("?123"));
        break;
      case SPECIAL_KEY_ALPHA:
        button->SetText(base::ASCIIToUTF16("ABC"));
        break;
      default:
        button->SetText(GetDisplayString(key.display_code,
                                         key.event_flags | event_flags()));
        break;
    }
    button->SetState(views::Button::STATE_NORMAL);
  }
}

views::View* KeyboardView::CreateButton() {
  views::LabelButton* button = new views::LabelButton(this, base::string16());
  button->SetTextColor(views::Button::STATE_NORMAL, SK_ColorWHITE);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->set_background(views::Background::CreateSolidBackground(78, 78, 78));
  button->SetFontList(button_font_);
  //  button->SetHaloColor(SK_ColorBLACK);
  // Turn off animations as we reuse buttons in different layouts. If we didn't
  // do this and you click a button to change the layout then the button you
  // clicked on would animate the transition even though it may now represent a
  // different key.
  button->SetAnimationDuration(0);
  return button;
}

views::LabelButton* KeyboardView::GetButton(int row, int column) {
  int offset = column;
  for (int i = 0; i < row; ++i)
    offset += static_cast<int>(rows_[i]->num_keys);
  return static_cast<views::LabelButton*>(child_at(offset));
}

const Key& KeyboardView::GetKeyForButton(views::Button* button) const {
  int index = GetIndexOf(button);
  DCHECK_NE(-1, index);
  int row = 0;
  while (index >= static_cast<int>(rows_[row]->num_keys)) {
    index -= static_cast<int>(rows_[row]->num_keys);
    row++;
  }
  return rows_[row]->keys[index];
}

void KeyboardView::ResetFonts(const gfx::FontList& button_font,
                              const gfx::FontList& special_font) {
  for (size_t i = 0; i < rows_.size(); ++i) {
    for (size_t j = 0; j < rows_[i]->num_keys; ++j) {
      views::LabelButton* button = GetButton(static_cast<int>(i),
                                             static_cast<int>(j));
      const Key& key(GetKeyForButton(button));
      switch (key.display_code) {
        case SPECIAL_KEY_ALPHA:
        case SPECIAL_KEY_NUMERIC:
          button->SetFontList(special_font);
          break;
        default:
          button->SetFontList(button_font);
          break;
      }
    }
  }
}

void KeyboardView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  const Key& key(GetKeyForButton(sender));
  switch (key.display_code) {
    case SPECIAL_KEY_SHIFT:
      SetLayout((keyboard_layout_ == KEYBOARD_LAYOUT_SHIFT) ?
                KEYBOARD_LAYOUT_ALPHA : KEYBOARD_LAYOUT_SHIFT);
      return;
    case SPECIAL_KEY_ALPHA:
      SetLayout(KEYBOARD_LAYOUT_ALPHA);
      return;
    case SPECIAL_KEY_NUMERIC:
      SetLayout(KEYBOARD_LAYOUT_NUMERIC);
      return;
    default:
      break;
  }

  // Windows isn't happy if we pass in the flags used to get the display string.
#if defined(OS_WIN)
  int key_event_flags = 0;
#else
  int key_event_flags = key.event_flags;
#endif
  delegate_->OnKeyPressed(key.keyboard_code(), key_event_flags | event_flags());
}

}  // namespace examples
}  // namespace mojo
