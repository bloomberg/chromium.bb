// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
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
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void RemoveSavedPassword(
      const std::string& origin_url, const std::string& username) override;
  void RemovePasswordException(const std::string& exception_url) override;
  void RequestShowPassword(const std::string& origin_url,
                           const std::string& username,
                           content::WebContents* web_contents) override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(
      size_t index,
      const std::string& origin_url,
      const std::string& username,
      const base::string16& plaintext_password) override;
  void SetPasswordList(
      const ScopedVector<autofill::PasswordForm>& password_list,
      bool show_passwords) override;
  void SetPasswordExceptionList(const ScopedVector<autofill::PasswordForm>&
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
  void ExecuteFunction(const base::Callback<void()>& callback);

  void RemoveSavedPasswordInternal(
      const std::string& origin_url, const std::string& username);
  void RemovePasswordExceptionInternal(const std::string& exception_url);
  void RequestShowPasswordInternal(const std::string& origin_url,
                                   const std::string& username,
                                   content::WebContents* web_contents);
  void SendSavedPasswordsList();
  void SendPasswordExceptionsList();

  // Not owned by this class.
  Profile* profile_;

  // Used to communicate with the password store.
  scoped_ptr<PasswordManagerPresenter> password_manager_presenter_;

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<linked_ptr<api::passwords_private::PasswordUiEntry>>
      current_entries_;
  std::vector<std::string> current_exceptions_;

  // Whether SetPasswordList and SetPasswordExceptionList have been called, and
  // whether this class has been initialized, meaning both have been called.
  bool set_password_list_called_;
  bool set_password_exception_list_called_;
  bool is_initialized_;

  // Vector of callbacks which are queued up before the password store has been
  // initialized. Once both SetPasswordList() and SetPasswordExceptionList()
  // have been called, this class is considered initialized and can these
  // callbacks are invoked.
  std::vector<base::Callback<void()>> pre_initialization_callbacks_;

  // User pref for storing accept languages.
  std::string languages_;

  // The WebContents used when invoking this API. Used to fetch the
  // NativeWindow for the window where the API was called.
  content::WebContents* web_contents_;

  // The observers.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // Map from origin URL and username to the index of |password_list_| at which
  // the corresponding entry resides.
  std::map<std::string, size_t> login_pair_to_index_map_;

  // Map from password exception URL to the index of |password_exception_list_|
  // at which the correponding entry resides.
  std::map<std::string, size_t> exception_url_to_index_map_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateDelegateImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
