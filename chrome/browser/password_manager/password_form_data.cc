// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"

using content::PasswordForm;

PasswordForm* CreatePasswordFormFromData(
    const PasswordFormData& form_data) {
  PasswordForm* form = new PasswordForm();
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->ssl_valid = form_data.ssl_valid;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = WideToUTF16(form_data.password_element);
  if (form_data.username_value) {
    form->username_value = WideToUTF16(form_data.username_value);
    if (form_data.password_value)
      form->password_value = WideToUTF16(form_data.password_value);
  } else {
    form->blacklisted_by_user = true;
  }
  return form;
}

bool operator==(const PasswordForm& lhs, const PasswordForm& rhs) {
  return (lhs.scheme == rhs.scheme &&
          lhs.signon_realm == rhs.signon_realm &&
          lhs.origin == rhs.origin &&
          lhs.action == rhs.action &&
          lhs.submit_element == rhs.submit_element &&
          lhs.username_element == rhs.username_element &&
          lhs.password_element == rhs.password_element &&
          lhs.username_value == rhs.username_value &&
          lhs.password_value == rhs.password_value &&
          lhs.blacklisted_by_user == rhs.blacklisted_by_user &&
          lhs.preferred == rhs.preferred &&
          lhs.ssl_valid == rhs.ssl_valid &&
          lhs.date_created == rhs.date_created);
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  return os << "scheme: " << form.scheme << std::endl
            << "signon_realm: " << form.signon_realm << std::endl
            << "origin: " << form.origin << std::endl
            << "action: " << form.action << std::endl
            << "submit_element: " << form.submit_element << std::endl
            << "username_elem: " << form.username_element << std::endl
            << "password_elem: " << form.password_element << std::endl
            << "username_value: " << form.username_value << std::endl
            << "password_value: " << form.password_value << std::endl
            << "blacklisted: " << form.blacklisted_by_user << std::endl
            << "preferred: " << form.preferred << std::endl
            << "ssl_valid: " << form.ssl_valid << std::endl
            << "date_created: " << form.date_created.ToDoubleT();
}

typedef std::set<const content::PasswordForm*> SetOfForms;

bool ContainsSamePasswordFormsPtr(
    const std::vector<PasswordForm*>& first,
    const std::vector<PasswordForm*>& second) {
  if (first.size() != second.size())
    return false;

  // TODO(cramya): As per b/7079906, the STLport of Android causes a crash
  // if we use expectations(first.begin(), first.end()) to copy a vector
  // into a set rather than std::copy that is used below.
  // Need to revert this once STLport is fixed.
  SetOfForms expectations;
  std::copy(first.begin(), first.end(), std::inserter(expectations,
                                                      expectations.begin()));
  for (unsigned int i = 0; i < second.size(); ++i) {
    const PasswordForm* actual = second[i];
    bool found_match = false;
    for (SetOfForms::iterator it = expectations.begin();
         it != expectations.end(); ++it) {
      const PasswordForm* expected = *it;
      if (*expected == *actual) {
        found_match = true;
        expectations.erase(it);
        break;
      }
    }
    if (!found_match) {
      LOG(ERROR) << "No match for:" << std::endl << *actual;
      return false;
    }
  }
  return true;
}

bool ContainsSamePasswordForms(
    std::vector<content::PasswordForm>& first,
    std::vector<content::PasswordForm>& second) {
  std::vector<PasswordForm*> first_ptr;
  for (unsigned int i = 0; i < first.size(); ++i) {
    first_ptr.push_back(&first[i]);
  }
  std::vector<PasswordForm*> second_ptr;
  for (unsigned int i = 0; i < second.size(); ++i) {
    second_ptr.push_back(&second[i]);
  }
  return ContainsSamePasswordFormsPtr(first_ptr, second_ptr);
}
