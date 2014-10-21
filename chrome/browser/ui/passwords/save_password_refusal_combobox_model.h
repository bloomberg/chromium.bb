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
  enum { INDEX_NOPE = 0, INDEX_NEVER_FOR_THIS_SITE = 1 };

  SavePasswordRefusalComboboxModel();
  ~SavePasswordRefusalComboboxModel() override;

 private:
  // Overridden from ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;

  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordRefusalComboboxModel);
};


#endif  // CHROME_BROWSER_UI_PASSWORDS_SAVE_PASSWORD_REFUSAL_COMBOBOX_MODEL_H_
