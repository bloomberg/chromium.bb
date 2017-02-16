// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_COMBOBOX_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_COMBOBOX_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "ui/views/controls/combobox/combobox.h"

namespace payments {

class ValidatingCombobox : public views::Combobox {
 public:
  ValidatingCombobox(std::unique_ptr<ui::ComboboxModel> model,
                     std::unique_ptr<ValidationDelegate> delegate);
  ~ValidatingCombobox() override;

  // Combobox:
  // The first validation will happen on blur.
  void OnBlur() override;

  // Called when the combobox contents is changed. May do validation.
  void OnContentsChanged();

 private:
  // Will call to the ValidationDelegate to validate the contents of the
  // combobox.
  void Validate();

  std::unique_ptr<ValidationDelegate> delegate_;
  bool was_blurred_;

  DISALLOW_COPY_AND_ASSIGN(ValidatingCombobox);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATING_COMBOBOX_H_
