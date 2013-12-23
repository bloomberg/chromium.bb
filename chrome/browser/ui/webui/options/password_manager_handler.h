// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_

#include <string>
#include <vector>

#include "base/prefs/pref_member.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// The WebUI based PasswordUIView. Displays passwords in the web ui.
class PasswordManagerHandler : public OptionsPageUIHandler,
                               public PasswordUIView {
 public:
  PasswordManagerHandler();
  virtual ~PasswordManagerHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // PasswordUIView implementation.
  virtual Profile* GetProfile() OVERRIDE;
  virtual void ShowPassword(size_t index, const base::string16& password_value)
      OVERRIDE;
  virtual void SetPasswordList(
      const ScopedVector<autofill::PasswordForm>& password_list,
      bool show_passwords) OVERRIDE;
  virtual void SetPasswordExceptionList(
      const ScopedVector<autofill::PasswordForm>& password_exception_list)
      OVERRIDE;
#if !defined(OS_ANDROID)
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
#endif
 private:
  // Clears and then populates the list of passwords and password exceptions.
  // Called when the JS PasswordManager object is initialized.
  void HandleUpdatePasswordLists(const base::ListValue* args);

  // Removes a saved password entry.
  // |value| the entry index to be removed.
  void HandleRemoveSavedPassword(const base::ListValue* args);

  // Removes a saved password exception.
  // |value| the entry index to be removed.
  void HandleRemovePasswordException(const base::ListValue* args);

  // Requests the plain text password for an entry to be revealed.
  // |index| The index of the entry.
  void HandleRequestShowPassword(const base::ListValue* args);

  // User pref for storing accept languages.
  std::string languages_;

  // The PasswordManagerPresenter object owned by the this view.
  PasswordManagerPresenter password_manager_presenter_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PASSWORD_MANAGER_HANDLER_H_
