// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill {

namespace {

// Serializes a PasswordForm to a JSON object. Used only for logging in tests.
void PasswordFormToJSON(const PasswordForm& form,
                        base::DictionaryValue* target) {
  target->SetInteger("scheme", form.scheme);
  target->SetString("signon_realm", form.signon_realm);
  target->SetString("signon_realm", form.signon_realm);
  target->SetString("original_signon_realm", form.original_signon_realm);
  target->SetString("origin", form.origin.possibly_invalid_spec());
  target->SetString("action", form.action.possibly_invalid_spec());
  target->SetString("submit_element", form.submit_element);
  target->SetString("username_elem", form.username_element);
  target->SetBoolean("username_marked_by_site", form.username_marked_by_site);
  target->SetString("username_value", form.username_value);
  target->SetString("password_elem", form.password_element);
  target->SetString("password_value", form.password_value);
  target->SetString("new_password_element", form.new_password_element);
  target->SetString("new_password_value", form.new_password_value);
  target->SetBoolean("new_password_marked_by_site",
                     form.new_password_marked_by_site);
  target->SetString("other_possible_usernames",
                    base::JoinString(form.other_possible_usernames,
                                     base::ASCIIToUTF16("|")));
  target->SetBoolean("blacklisted", form.blacklisted_by_user);
  target->SetBoolean("preferred", form.preferred);
  target->SetBoolean("ssl_valid", form.ssl_valid);
  target->SetDouble("date_created", form.date_created.ToDoubleT());
  target->SetDouble("date_synced", form.date_synced.ToDoubleT());
  target->SetInteger("type", form.type);
  target->SetInteger("times_used", form.times_used);
  std::ostringstream form_data_string_stream;
  form_data_string_stream << form.form_data;
  target->SetString("form_data", form_data_string_stream.str());
  target->SetInteger("generation_upload_status", form.generation_upload_status);
  target->SetString("display_name", form.display_name);
  target->SetString("icon_url", form.icon_url.possibly_invalid_spec());
  target->SetString("federation_url",
                    form.federation_url.possibly_invalid_spec());
  target->SetBoolean("skip_next_zero_click", form.skip_zero_click);
  std::ostringstream layout_string_stream;
  layout_string_stream << form.layout;
  target->SetString("layout", layout_string_stream.str());
  target->SetBoolean("was_parsed_using_autofill_predictions",
                     form.was_parsed_using_autofill_predictions);
}

}  // namespace

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      username_marked_by_site(false),
      new_password_marked_by_site(false),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false),
      type(TYPE_MANUAL),
      times_used(0),
      generation_upload_status(NO_SIGNAL_SENT),
      skip_zero_click(false),
      layout(Layout::LAYOUT_OTHER),
      was_parsed_using_autofill_predictions(false),
      is_alive(true) {
}

PasswordForm::~PasswordForm() {
  CHECK(is_alive);
  is_alive = false;
}

bool PasswordForm::IsPublicSuffixMatch() const {
  CHECK(is_alive);
  return !original_signon_realm.empty();
}

bool PasswordForm::IsPossibleChangePasswordForm() const {
  return !new_password_element.empty() &&
         layout != PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP;
}

bool PasswordForm::IsPossibleChangePasswordFormWithoutUsername() const {
  return IsPossibleChangePasswordForm() && username_element.empty();
}

bool PasswordForm::operator==(const PasswordForm& form) const {
  return scheme == form.scheme &&
      signon_realm == form.signon_realm &&
      original_signon_realm == form.original_signon_realm &&
      origin == form.origin &&
      action == form.action &&
      submit_element == form.submit_element &&
      username_element == form.username_element &&
      username_marked_by_site == form.username_marked_by_site &&
      username_value == form.username_value &&
      other_possible_usernames == form.other_possible_usernames &&
      password_element == form.password_element &&
      password_value == form.password_value &&
      new_password_element == form.new_password_element &&
      new_password_marked_by_site == form.new_password_marked_by_site &&
      new_password_value == form.new_password_value &&
      ssl_valid == form.ssl_valid &&
      preferred == form.preferred &&
      date_created == form.date_created &&
      date_synced == form.date_synced &&
      blacklisted_by_user == form.blacklisted_by_user &&
      type == form.type &&
      times_used == form.times_used &&
      form_data.SameFormAs(form.form_data) &&
      generation_upload_status == form.generation_upload_status &&
      display_name == form.display_name &&
      icon_url == form.icon_url &&
      federation_url == form.federation_url &&
      skip_zero_click == form.skip_zero_click &&
      layout == form.layout &&
      was_parsed_using_autofill_predictions ==
          form.was_parsed_using_autofill_predictions;
}

bool PasswordForm::operator!=(const PasswordForm& form) const {
  return !operator==(form);
}

std::ostream& operator<<(std::ostream& os, PasswordForm::Layout layout) {
  switch (layout) {
    case PasswordForm::Layout::LAYOUT_OTHER:
      os << "LAYOUT_OTHER";
      break;
    case PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP:
      os << "LAYOUT_LOGIN_AND_SIGNUP";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  base::DictionaryValue form_json;
  PasswordFormToJSON(form, &form_json);

  // Serialize the default PasswordForm, and remove values from the result that
  // are equal to this to make the results more concise.
  base::DictionaryValue default_form_json;
  PasswordFormToJSON(PasswordForm(), &default_form_json);
  for (base::DictionaryValue::Iterator it_default_key_values(default_form_json);
       !it_default_key_values.IsAtEnd(); it_default_key_values.Advance()) {
    const base::Value* actual_value;
    if (form_json.Get(it_default_key_values.key(), &actual_value) &&
        it_default_key_values.value().Equals(actual_value)) {
      form_json.Remove(it_default_key_values.key(), nullptr);
    }
  }

  std::string form_as_string;
  base::JSONWriter::WriteWithOptions(
      form_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &form_as_string);
  base::TrimWhitespaceASCII(form_as_string, base::TRIM_ALL, &form_as_string);
  return os << "PasswordForm(" << form_as_string << ")";
}

std::ostream& operator<<(std::ostream& os, PasswordForm* form) {
  return os << "&" << *form;
}

}  // namespace autofill
