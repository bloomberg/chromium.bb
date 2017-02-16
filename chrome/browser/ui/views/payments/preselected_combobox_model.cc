// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/preselected_combobox_model.h"

namespace payments {

PreselectedComboboxModel::PreselectedComboboxModel(
    const std::vector<base::string16>& items,
    int default_index)
    : SimpleComboboxModel(items), default_index_(default_index) {}

PreselectedComboboxModel::~PreselectedComboboxModel() {}

int PreselectedComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

}  // namespace payments
