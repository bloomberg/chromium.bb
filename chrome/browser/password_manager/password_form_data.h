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

// Checks whether two vectors of PasswordForms contain equivalent elements,
// regardless of order.
bool ContainsSamePasswordFormsPtr(
    const std::vector<webkit_glue::PasswordForm*>& first,
    const std::vector<webkit_glue::PasswordForm*>& second);

bool ContainsSamePasswordForms(
    std::vector<webkit_glue::PasswordForm>& first,
    std::vector<webkit_glue::PasswordForm>& second);

// This gmock matcher is used to check that the |arg| contains exactly the same
// PasswordForms as |forms|, regardless of order.
MATCHER_P(ContainsAllPasswordForms, forms, "") {
  return ContainsSamePasswordFormsPtr(forms, arg);
}

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FORM_DATA_H_
