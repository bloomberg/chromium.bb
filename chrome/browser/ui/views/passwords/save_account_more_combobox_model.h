// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_SAVE_ACCOUNT_MORE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_SAVE_ACCOUNT_MORE_COMBOBOX_MODEL_H_

#include <vector>
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"

class SaveAccountMoreComboboxModel : public ui::ComboboxModel {
 public:
  // The order of the values shouldn't be changed without reordering the strings
  // in the constructor.
  enum {
    INDEX_MORE = 0,
    INDEX_NEVER_FOR_THIS_SITE,
    INDEX_SETTINGS,
  };

  SaveAccountMoreComboboxModel();
  ~SaveAccountMoreComboboxModel() override;

 private:
  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SaveAccountMoreComboboxModel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_SAVE_ACCOUNT_MORE_COMBOBOX_MODEL_H_
