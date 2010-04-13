// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"

using webkit_glue::PasswordForm;

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
