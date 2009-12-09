// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager.h"

#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/form_structure.h"

PersonalDataManager::~PersonalDataManager() {
}

bool PersonalDataManager::ImportFormData(
    const std::vector<FormStructure*>& form_structures,
    AutoFillManager* autofill_manager) {
  return true;
}

void PersonalDataManager::GetPossibleFieldTypes(const string16& text,
                                                FieldTypeSet* possible_types) {
}

PersonalDataManager::PersonalDataManager() {
}
