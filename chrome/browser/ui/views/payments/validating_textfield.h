// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_

#include "base/macros.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfield : public views::Textfield {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Only the delegate knows how to validate the textfield.
    virtual bool ValidateTextfield(views::Textfield* textfield) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  explicit ValidatingTextfield(
      std::unique_ptr<ValidatingTextfield::Delegate> delegate);
  ~ValidatingTextfield() override;

  // Textfield:
  // The first validation will happen on blur.
  void OnBlur() override;

  // Called when the textfield contents is changed. May do validation.
  void OnContentsChanged();

 private:
  // Will call to the Delegate to validate the contents of the textfield.
  void Validate();

  std::unique_ptr<ValidatingTextfield::Delegate> delegate_;
  bool was_validated_ = false;

  DISALLOW_COPY_AND_ASSIGN(ValidatingTextfield);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_TEXTFIELD_H_
