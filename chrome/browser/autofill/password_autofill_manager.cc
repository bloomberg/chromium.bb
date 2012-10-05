// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/password_autofill_manager.h"
#include "chrome/common/autofill_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/keycodes/keyboard_codes.h"

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    content::WebContents* web_contents) : web_contents_(web_contents) {
}

PasswordAutofillManager::~PasswordAutofillManager() {
}

bool PasswordAutofillManager::DidAcceptAutofillSuggestion(
    const FormFieldData& field,
    const string16& value) {
  PasswordFormFillData password;
  if (!FindLoginInfo(field, &password))
    return false;

  if (WillFillUserNameAndPassword(value, password)) {
    if (web_contents_) {
      content::RenderViewHost* render_view_host =
          web_contents_->GetRenderViewHost();
      render_view_host->Send(new AutofillMsg_AcceptPasswordAutofillSuggestion(
          render_view_host->GetRoutingID(),
          value));
    }
    return true;
  }

  return false;
}

void PasswordAutofillManager::AddPasswordFormMapping(
    const FormFieldData& username_element,
    const PasswordFormFillData& password) {
  login_to_password_info_[username_element] = password;
}

void PasswordAutofillManager::Reset() {
  login_to_password_info_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

bool PasswordAutofillManager::WillFillUserNameAndPassword(
    const string16& current_username,
    const PasswordFormFillData& fill_data) {
  // Look for any suitable matches to current field text.
  if (fill_data.basic_data.fields[0].value == current_username) {
    return true;
  } else {
    // Scan additional logins for a match.
    PasswordFormFillData::LoginCollection::const_iterator iter;
    for (iter = fill_data.additional_logins.begin();
         iter != fill_data.additional_logins.end(); ++iter) {
      if (iter->first == current_username)
        return true;
    }
  }

  return false;
}

bool PasswordAutofillManager::FindLoginInfo(
    const FormFieldData& field,
    PasswordFormFillData* found_password) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(field);
  if (iter == login_to_password_info_.end())
    return false;

  *found_password = iter->second;
  return true;
}
