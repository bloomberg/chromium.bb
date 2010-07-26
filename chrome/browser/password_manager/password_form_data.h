// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FORM_DATA_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FORM_DATA_H_
#pragma once

#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/password_form.h"

// Struct used for creation of PasswordForms from static arrays of data.
// Note: This is only meant to be used in unit test.
struct PasswordFormData {
  const webkit_glue::PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const wchar_t* submit_element;
  const wchar_t* username_element;
  const wchar_t* password_element;
  const wchar_t* username_value;  // Set to NULL for a blacklist entry.
  const wchar_t* password_value;
  const bool preferred;
  const bool ssl_valid;
  const double creation_time;
};

// Creates and returns a new PasswordForm built from form_data. Caller is
// responsible for deleting the object when finished with it.
webkit_glue::PasswordForm* CreatePasswordFormFromData(
    const PasswordFormData& form_data);

typedef std::set<webkit_glue::PasswordForm*> SetOfForms;

// This gmock matcher is used to check that the |arg| contains exactly the same
// PasswordForms as |forms|, regardless of order.
MATCHER_P(ContainsAllPasswordForms, forms, "") {
  if (forms.size() != arg.size())
    return false;
  SetOfForms expectations(forms.begin(), forms.end());
  for (unsigned int i = 0; i < arg.size(); ++i) {
    webkit_glue::PasswordForm* actual = arg[i];
    bool found_match = false;
    for (SetOfForms::iterator it = expectations.begin();
         it != expectations.end(); ++it) {
      webkit_glue::PasswordForm* expected = *it;
      if (expected->scheme == actual->scheme &&
          expected->signon_realm == actual->signon_realm &&
          expected->origin == actual->origin &&
          expected->action == actual->action &&
          expected->submit_element == actual->submit_element &&
          expected->username_element == actual->username_element &&
          expected->password_element == actual->password_element &&
          expected->username_value == actual->username_value &&
          expected->password_value == actual->password_value &&
          expected->blacklisted_by_user == actual->blacklisted_by_user &&
          expected->preferred == actual->preferred &&
          expected->ssl_valid == actual->ssl_valid &&
          expected->date_created == actual->date_created) {
        found_match = true;
        expectations.erase(it);
        break;
      }
    }
    if (!found_match) {
      LOG(ERROR) << "No match for:" << std::endl
                 << "scheme: " << actual->scheme << std::endl
                 << "signon_realm: " << actual->signon_realm << std::endl
                 << "origin: " << actual->origin << std::endl
                 << "action: " << actual->action << std::endl
                 << "submit_element: " << actual->submit_element << std::endl
                 << "username_elem: " << actual->username_element << std::endl
                 << "password_elem: " << actual->password_element << std::endl
                 << "username_value: " << actual->username_value << std::endl
                 << "password_value: " << actual->password_value << std::endl
                 << "blacklisted: " << actual->blacklisted_by_user << std::endl
                 << "preferred: " << actual->preferred << std::endl
                 << "ssl_valid: " << actual->ssl_valid << std::endl
                 << "date_created: " << actual->date_created.ToDoubleT();
      return false;
    }
  }
  return true;
}

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FORM_DATA_H_
