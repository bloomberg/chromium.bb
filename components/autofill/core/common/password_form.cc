// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill {

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      password_autocomplete_set(true),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false),
      type(TYPE_MANUAL),
      times_used(0),
      use_additional_authentication(false),
      is_zero_click(false) {
}

PasswordForm::~PasswordForm() {
}

bool PasswordForm::IsPublicSuffixMatch() const {
  return !original_signon_realm.empty();
}

bool PasswordForm::operator==(const PasswordForm& form) const {
  return signon_realm == form.signon_realm &&
      origin == form.origin &&
      action == form.action &&
      submit_element == form.submit_element &&
      username_element == form.username_element &&
      username_value == form.username_value &&
      other_possible_usernames == form.other_possible_usernames &&
      password_element == form.password_element &&
      password_value == form.password_value &&
      password_autocomplete_set == form.password_autocomplete_set &&
      new_password_element == form.new_password_element &&
      new_password_value == form.new_password_value &&
      ssl_valid == form.ssl_valid &&
      preferred == form.preferred &&
      date_created == form.date_created &&
      date_synced == form.date_synced &&
      blacklisted_by_user == form.blacklisted_by_user &&
      type == form.type &&
      times_used == form.times_used &&
      use_additional_authentication == form.use_additional_authentication &&
      form_data == form.form_data &&
      display_name == form.display_name &&
      avatar_url == form.avatar_url &&
      federation_url == form.federation_url &&
      is_zero_click == form.is_zero_click;
}

bool PasswordForm::operator!=(const PasswordForm& form) const {
  return !operator==(form);
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  return os << "scheme: " << form.scheme
            << " signon_realm: " << form.signon_realm
            << " origin: " << form.origin
            << " action: " << form.action
            << " submit_element: " << base::UTF16ToUTF8(form.submit_element)
            << " username_elem: " << base::UTF16ToUTF8(form.username_element)
            << " username_value: " << base::UTF16ToUTF8(form.username_value)
            << " password_elem: " << base::UTF16ToUTF8(form.password_element)
            << " password_value: " << base::UTF16ToUTF8(form.password_value)
            << " new_password_element: "
            << base::UTF16ToUTF8(form.new_password_element)
            << " new_password_value: "
            << base::UTF16ToUTF8(form.new_password_value)
            << " autocomplete_set:" << form.password_autocomplete_set
            << " blacklisted: " << form.blacklisted_by_user
            << " preferred: " << form.preferred
            << " ssl_valid: " << form.ssl_valid
            << " date_created: " << form.date_created.ToDoubleT()
            << " date_synced: " << form.date_synced.ToDoubleT()
            << " type: " << form.type
            << " times_used: " << form.times_used
            << " use additional authentication: "
            << form.use_additional_authentication
            << " form_data: " << form.form_data
            << " display_name: " << base::UTF16ToUTF8(form.display_name)
            << " avatar_url: " << form.avatar_url
            << " federation_url: " << form.federation_url
            << " is_zero_click: " << form.is_zero_click;
}

}  // namespace autofill
