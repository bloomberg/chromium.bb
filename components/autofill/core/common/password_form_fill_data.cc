// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

PasswordFormFillData::PasswordFormFillData() : wait_for_username(false) {}

PasswordFormFillData::PasswordFormFillData(const PasswordFormFillData& other) =
    default;

PasswordFormFillData::~PasswordFormFillData() = default;

void InitPasswordFormFillData(
    const PasswordForm& form_on_page,
    const std::map<base::string16, const PasswordForm*>& matches,
    const PasswordForm* const preferred_match,
    bool wait_for_username_before_autofill,
    PasswordFormFillData* result) {
  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  FormFieldData username_field;
  username_field.value = preferred_match->username_value;
  FormFieldData password_field;
  password_field.value = preferred_match->password_value;
  if (!form_on_page.only_for_fallback) {
    // Fill fields identifying information only for non-fallback case. In
    // fallback case, a fill popup is shown on clicking on each password
    // field so no need in any field identifiers.
    username_field.name = form_on_page.username_element;
    username_field.unique_renderer_id =
        form_on_page.username_element_renderer_id;
    result->username_may_use_prefilled_placeholder =
        form_on_page.username_may_use_prefilled_placeholder;

    password_field.name = form_on_page.password_element;
    password_field.unique_renderer_id =
        form_on_page.password_element_renderer_id;
    password_field.form_control_type = "password";

    // On iOS, use the unique_id field to refer to elements.
#if defined(OS_IOS)
    username_field.unique_id = form_on_page.username_element;
    password_field.unique_id = form_on_page.password_element;
#endif
  }

  // Fill basic form data.
  result->form_renderer_id = form_on_page.form_data.unique_renderer_id;
  result->name = form_on_page.form_data.name;
  result->origin = form_on_page.origin;
  result->action = form_on_page.action;
  result->username_field = username_field;
  result->password_field = password_field;
  result->wait_for_username = wait_for_username_before_autofill;
  result->has_renderer_ids = form_on_page.has_renderer_ids;

  if (preferred_match->is_public_suffix_match ||
      preferred_match->is_affiliation_based_match)
    result->preferred_realm = preferred_match->signon_realm;

  // Copy additional username/value pairs.
  for (const auto& it : matches) {
    if (it.second != preferred_match) {
      PasswordAndRealm value;
      value.password = it.second->password_value;
      if (it.second->is_public_suffix_match ||
          it.second->is_affiliation_based_match)
        value.realm = it.second->signon_realm;
      result->additional_logins[it.first] = value;
    }
  }
}

PasswordFormFillData MaybeClearPasswordValues(
    const PasswordFormFillData& data) {
  // In case when there is a username on a page (for example in a hidden field),
  // credentials from |additional_logins| could be used for filling on load. So
  // in case of filling on load nor |password_field| nor |additional_logins|
  // can't be cleared
  bool is_fallback =
      data.has_renderer_ids && data.password_field.unique_renderer_id ==
                                   FormFieldData::kNotSetFormControlRendererId;
  if (!data.wait_for_username && !is_fallback)
    return data;
  PasswordFormFillData result(data);
  result.password_field.value.clear();
  for (auto& credentials : result.additional_logins)
    credentials.second.password.clear();
  return result;
}

}  // namespace autofill
