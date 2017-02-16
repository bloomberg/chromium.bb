// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PRESELECTED_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PRESELECTED_COMBOBOX_MODEL_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/models/simple_combobox_model.h"

namespace payments {

// A simple data model for a combobox that takes a string16 vector as the items,
// as well as the default selected index. An empty string is a separator.
class PreselectedComboboxModel : public ui::SimpleComboboxModel {
 public:
  PreselectedComboboxModel(const std::vector<base::string16>& items,
                           int default_index);
  ~PreselectedComboboxModel() override;

  // ui::ComboboxModel:
  int GetDefaultIndex() const override;

 private:
  const int default_index_;

  DISALLOW_COPY_AND_ASSIGN(PreselectedComboboxModel);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PRESELECTED_COMBOBOX_MODEL_H_
