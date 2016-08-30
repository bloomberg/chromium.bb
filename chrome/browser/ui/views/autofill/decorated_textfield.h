// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/textfield/textfield.h"

namespace views {
class TextfieldController;
}

namespace autofill {

// A class which holds a textfield and draws extra stuff on top, like
// invalid content indications.
// TODO(estade): the usefulness of this class is dubious now that it's been
// stripped of most of its functionality.
class DecoratedTextfield : public views::Textfield {
 public:
  static const char kViewClassName[];

  DecoratedTextfield(const base::string16& default_value,
                     const base::string16& placeholder,
                     views::TextfieldController* controller);
  ~DecoratedTextfield() override;

  // Sets whether to indicate the textfield has invalid content.
  void SetInvalid(bool invalid);
  bool invalid() const { return invalid_; }

  // views::View implementation.
  const char* GetClassName() const override;

 private:
  // Updates the border after its color or insets may have changed.
  void UpdateBorder();

  // Whether the text contents are "invalid" (i.e. should special markers be
  // shown to indicate invalidness).
  bool invalid_;

  DISALLOW_COPY_AND_ASSIGN(DecoratedTextfield);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_DECORATED_TEXTFIELD_H_
