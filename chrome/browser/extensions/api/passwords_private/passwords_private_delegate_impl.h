// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// Concrete PasswordsPrivateDelegate implementation.
class PasswordsPrivateDelegateImpl : public PasswordsPrivateDelegate,
                                     public PasswordUIView  {
 public:
  explicit PasswordsPrivateDelegateImpl(Profile* profile);
  ~PasswordsPrivateDelegateImpl() override;

  // PasswordsPrivateDelegate implementation.
  void SendSavedPasswordsList() override;
  void GetSavedPasswordsList(const UiEntriesCallback& callback) override;
  void SendPasswordExceptionsList() override;
  void GetPasswordExceptionsList(
      const ExceptionEntriesCallback& callback) override;
  void RemoveSavedPassword(size_t index) override;
  void RemovePasswordException(size_t index) override;
  void UndoRemoveSavedPasswordOrException() override;
  void RequestShowPassword(size_t index,
                           content::WebContents* web_contents) override;
  void ImportPasswords(content::WebContents* web_contents) override;
  void ExportPasswords(content::WebContents* web_contents) override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(
      size_t index,
      const base::string16& plaintext_password) override;
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list)
      override;
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list) override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  // KeyedService overrides:
  void Shutdown() override;

 private:
  // Called after the lists are fetched. Once both lists have been set, the
  // class is considered initialized and any queued functions (which could
  // not be executed immediately due to uninitialized data) are invoked.
  void InitializeIfNecessary();

  // Executes a given callback by either invoking it immediately if the class
  // has been initialized or by deferring it until initialization has completed.
  void ExecuteFunction(const base::Closure& callback);

  void RemoveSavedPasswordInternal(size_t index);
  void RemovePasswordExceptionInternal(size_t index);
  void UndoRemoveSavedPasswordOrExceptionInternal();
  void RequestShowPasswordInternal(size_t index,
                                   content::WebContents* web_contents);

  // Not owned by this class.
  Profile* profile_;

  // Used to communicate with the password store.
  std::unique_ptr<PasswordManagerPresenter> password_manager_presenter_;

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  UiEntries current_entries_;
  ExceptionEntries current_exceptions_;

  // Whether SetPasswordList and SetPasswordExceptionList have been called, and
  // whether this class has been initialized, meaning both have been called.
  bool current_entries_initialized_;
  bool current_exceptions_initialized_;
  bool is_initialized_;

  // Vector of callbacks which are queued up before the password store has been
  // initialized. Once both SetPasswordList() and SetPasswordExceptionList()
  // have been called, this class is considered initialized and can these
  // callbacks are invoked.
  std::vector<base::Closure> pre_initialization_callbacks_;
  std::vector<UiEntriesCallback> get_saved_passwords_list_callbacks_;
  std::vector<ExceptionEntriesCallback> get_password_exception_list_callbacks_;

  // The WebContents used when invoking this API. Used to fetch the
  // NativeWindow for the window where the API was called.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateDelegateImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
