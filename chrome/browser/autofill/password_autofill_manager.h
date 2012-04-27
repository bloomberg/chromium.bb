// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
#pragma once

// This file was contains some repeated code from
// chrome/renderer/autofill/password_autofill_manager because as we move to the
// new Autofill UI we needs these functions in both the browser and renderer.
// Once the move is completed the repeated code in the renderer half should be
// removed.
// http://crbug.com/51644

#include <map>

#include "webkit/forms/password_form_dom_manager.h"

namespace content {
class WebContents;
}  // namespace content

// This class is responsible for filling password forms.
class PasswordAutofillManager {
 public:
  explicit PasswordAutofillManager(content::WebContents* web_contents);
  virtual ~PasswordAutofillManager();

  // Fills the password associated with user name |value|. Returns true if the
  // username and password fields were filled, false otherwise.
  bool DidAcceptAutofillSuggestion(const webkit::forms::FormField& field,
                                   const string16& value);

  // Invoked when a password mapping is added.
  void AddPasswordFormMapping(
      const webkit::forms::FormField& username_element,
      const webkit::forms::PasswordFormFillData& password);

  // Invoked to clear any page specific cached values.
  void Reset();

 private:
  // TODO(csharp): Modify the AutofillExternalDeletegate code so that it can
  // figure out if a entry is a password one without using this mapping.
  // crbug.com/118601
  typedef std::map<webkit::forms::FormField,
                   webkit::forms::PasswordFormFillData>
      LoginToPasswordInfoMap;

  // Returns true if |current_username| matches a username for one of the
  // login mappings in |password|.
  bool WillFillUserNameAndPassword(
      const string16& current_username,
      const webkit::forms::PasswordFormFillData& password);

  // Finds login information for a |node| that was previously filled.
  bool FindLoginInfo(const webkit::forms::FormField& field,
                     webkit::forms::PasswordFormFillData* found_password);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  // We only need the RenderViewHost pointer in WebContents, but if we attempt
  // to just store RenderViewHost on creation, it becomes invalid once we start
  // using it. By having the WebContents we can always get a valid pointer.
  content::WebContents* web_contents_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
