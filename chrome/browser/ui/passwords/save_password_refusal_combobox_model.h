// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SAVE_PASSWORD_REFUSAL_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_SAVE_PASSWORD_REFUSAL_COMBOBOX_MODEL_H_

#include <vector>
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"

class SavePasswordRefusalComboboxModel : public ui::ComboboxModel {
 public:
  explicit SavePasswordRefusalComboboxModel(bool never_is_default);
  ~SavePasswordRefusalComboboxModel() override;

  int index_nope() const {
    return never_is_default_ ? 1 : 0;
  }

  int index_never() const {
    return never_is_default_ ? 0 : 1;
  }

  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;

 private:
  std::vector<base::string16> items_;
  const bool never_is_default_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordRefusalComboboxModel);
};


#endif  // CHROME_BROWSER_UI_PASSWORDS_SAVE_PASSWORD_REFUSAL_COMBOBOX_MODEL_H_
