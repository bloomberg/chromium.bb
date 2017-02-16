// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_

namespace views {
class Combobox;
class Textfield;
}  // namespace views

namespace payments {

class ValidationDelegate {
 public:
  virtual ~ValidationDelegate() {}

  // Only the delegate knows how to validate the textfield.
  virtual bool ValidateTextfield(views::Textfield* textfield) = 0;

  virtual bool ValidateCombobox(views::Combobox* combobox) = 0;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_
