// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_MORE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_MORE_COMBOBOX_MODEL_H_

#include "ui/base/models/combobox_model.h"

// Model for the account chooser's "More" combobox.
class AccountChooserMoreComboboxModel : public ui::ComboboxModel {
 public:
  AccountChooserMoreComboboxModel();
  ~AccountChooserMoreComboboxModel() override;

  // Index constants.
  static const int INDEX_MORE;
  static const int INDEX_LEARN_MORE;
  static const int INDEX_SETTINGS;

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;
  bool IsItemEnabledAt(int index) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountChooserMoreComboboxModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_MORE_COMBOBOX_MODEL_H_
