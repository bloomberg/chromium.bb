// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/common/password_form_fill_data.h"

#include "base/logging.h"
#include "components/autofill/common/form_field_data.h"

PasswordFormFillData::PasswordFormFillData() : wait_for_username(false) {
}

PasswordFormFillData::~PasswordFormFillData() {
}

void InitPasswordFormFillData(
    const content::PasswordForm& form_on_page,
    const content::PasswordFormMap& matches,
    const content::PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    PasswordFormFillData* result) {
  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  FormFieldData username_field;
  username_field.name = form_on_page.username_element;
  username_field.value = preferred_match->username_value;
  FormFieldData password_field;
  password_field.name = form_on_page.password_element;
  password_field.value = preferred_match->password_value;

  // Fill basic form data.
  result->basic_data.origin = form_on_page.origin;
  result->basic_data.action = form_on_page.action;
  result->basic_data.fields.push_back(username_field);
  result->basic_data.fields.push_back(password_field);
  result->wait_for_username = wait_for_username_before_autofill;

  // Copy additional username/value pairs.
  content::PasswordFormMap::const_iterator iter;
  for (iter = matches.begin(); iter != matches.end(); iter++) {
    if (iter->second != preferred_match)
      result->additional_logins[iter->first] = iter->second->password_value;
  }
}
