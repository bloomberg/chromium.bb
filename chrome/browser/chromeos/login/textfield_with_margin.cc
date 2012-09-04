// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/textfield_with_margin.h"

#include "chrome/browser/chromeos/login/helper.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace {

// Holds ratio of the margin to the preferred text height.
const double kTextMarginRate = 0.33;

// Size of each vertical margin (top, bottom).
const int kVerticalMargin = 3;

}  // namespace

namespace chromeos {

TextfieldWithMargin::TextfieldWithMargin() {
  CorrectTextfieldFontSize(this);
}

TextfieldWithMargin::TextfieldWithMargin(views::Textfield::StyleFlags style)
    : Textfield(style) {
  CorrectTextfieldFontSize(this);
}

void TextfieldWithMargin::Layout() {
  int margin = GetPreferredSize().height() * kTextMarginRate;
  SetHorizontalMargins(margin, margin);
  SetVerticalMargins(kVerticalMargin, kVerticalMargin);
  views::Textfield::Layout();
}

bool TextfieldWithMargin::OnKeyPressed(const ui::KeyEvent& e) {
  if (e.key_code() == ui::VKEY_ESCAPE && !text().empty()) {
    SetText(string16());
    return true;
  }
  return views::Textfield::OnKeyPressed(e);
}

}  // namespace chromeos
