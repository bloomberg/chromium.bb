// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_test_utils.h"

#include <set>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using autofill::PasswordForm;

namespace password_manager {

const char kTestingAvatarUrlSpec[] = "https://accounts.google.com/Avatar";
const char kTestingFederationUrlSpec[] = "https://accounts.google.com/login";
const int kTestingDaysAfterPasswordsAreSynced = 1;

scoped_ptr<PasswordForm> CreatePasswordFormFromDataForTesting(
    const PasswordFormData& form_data) {
  scoped_ptr<PasswordForm> form(new PasswordForm());
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->ssl_valid = form_data.ssl_valid;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  form->date_synced =
      form->date_created +
      base::TimeDelta::FromDays(kTestingDaysAfterPasswordsAreSynced);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = base::WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = base::WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = base::WideToUTF16(form_data.password_element);
  if (form_data.username_value) {
    form->username_value = base::WideToUTF16(form_data.username_value);
    form->display_name = form->username_value;
    form->skip_zero_click = true;
    if (form_data.password_value)
      form->password_value = base::WideToUTF16(form_data.password_value);
  } else {
    form->blacklisted_by_user = true;
  }
  form->avatar_url = GURL(kTestingAvatarUrlSpec);
  form->federation_url = GURL(kTestingFederationUrlSpec);
  return form.Pass();
}

typedef std::set<const autofill::PasswordForm*> SetOfForms;

bool ContainsSamePasswordFormsPtr(const std::vector<PasswordForm*>& first,
                                  const std::vector<PasswordForm*>& second) {
  if (first.size() != second.size())
    return false;

  // TODO(cramya): As per b/7079906, the STLport of Android causes a crash
  // if we use expectations(first.begin(), first.end()) to copy a vector
  // into a set rather than std::copy that is used below.
  // Need to revert this once STLport is fixed.
  SetOfForms expectations;
  std::copy(first.begin(), first.end(),
            std::inserter(expectations, expectations.begin()));
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

}  // namespace password_manager
