// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/decorated_textfield.h"

#include <utility>

#include "chrome/browser/ui/views/autofill/tooltip_icon.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_targeter.h"

namespace autofill {

// static
const char DecoratedTextfield::kViewClassName[] = "autofill/DecoratedTextfield";

DecoratedTextfield::DecoratedTextfield(
    const base::string16& default_value,
    const base::string16& placeholder,
    views::TextfieldController* controller)
    : invalid_(false) {
  UpdateBorder();

  set_placeholder_text(placeholder);
  SetText(default_value);
  set_controller(controller);
}

DecoratedTextfield::~DecoratedTextfield() {}

void DecoratedTextfield::SetInvalid(bool invalid) {
  if (invalid_ == invalid)
    return;

  invalid_ = invalid;
  UpdateBorder();
  SchedulePaint();
}

const char* DecoratedTextfield::GetClassName() const {
  return kViewClassName;
}

void DecoratedTextfield::UpdateBorder() {
  std::unique_ptr<views::FocusableBorder> border(new views::FocusableBorder());
  if (invalid_)
    border->SetColor(gfx::kGoogleRed700);

  SetBorder(std::move(border));
}

} // namespace autofill
