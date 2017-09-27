// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      password_manager_presenter_(new PasswordManagerPresenter(this)),
      current_entries_initialized_(false),
      current_exceptions_initialized_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  password_manager_presenter_->Initialize();
  password_manager_presenter_->UpdatePasswordLists();
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {}

void PasswordsPrivateDelegateImpl::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void PasswordsPrivateDelegateImpl::GetSavedPasswordsList(
    const UiEntriesCallback& callback) {
  if (current_entries_initialized_)
    callback.Run(current_entries_);
  else
    get_saved_passwords_list_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

void PasswordsPrivateDelegateImpl::GetPasswordExceptionsList(
    const ExceptionEntriesCallback& callback) {
  if (current_exceptions_initialized_)
    callback.Run(current_exceptions_);
  else
    get_password_exception_list_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::RemoveSavedPassword(size_t index) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal,
                 base::Unretained(this), index));
}

void PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal(size_t index) {
  password_manager_presenter_->RemoveSavedPassword(index);
}

void PasswordsPrivateDelegateImpl::RemovePasswordException(size_t index) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal,
                 base::Unretained(this), index));
}

void PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal(
    size_t index) {
  password_manager_presenter_->RemovePasswordException(index);
}

void PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrException() {
  ExecuteFunction(base::Bind(
      &PasswordsPrivateDelegateImpl::UndoRemoveSavedPasswordOrExceptionInternal,
      base::Unretained(this)));
}

void PasswordsPrivateDelegateImpl::
    UndoRemoveSavedPasswordOrExceptionInternal() {
  password_manager_presenter_->UndoRemoveSavedPasswordOrException();
}

void PasswordsPrivateDelegateImpl::RequestShowPassword(
    size_t index,
    content::WebContents* web_contents) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RequestShowPasswordInternal,
                 base::Unretained(this), index, web_contents));
}

void PasswordsPrivateDelegateImpl::RequestShowPasswordInternal(
    size_t index,
    content::WebContents* web_contents) {
  // Save |web_contents| so that the call to RequestShowPassword() below
  // can use this value by calling GetNativeWindow(). Note: This is safe because
  // GetNativeWindow() will only be called immediately from
  // RequestShowPassword().
  // TODO(stevenjb): Pass this directly to RequestShowPassword(); see
  // crbug.com/495290.
  web_contents_ = web_contents;

  // Request the password. When it is retrieved, ShowPassword() will be called.
  password_manager_presenter_->RequestShowPassword(index);
}

Profile* PasswordsPrivateDelegateImpl::GetProfile() {
  return profile_;
}

void PasswordsPrivateDelegateImpl::ShowPassword(
    size_t index,
    const base::string16& password_value) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPlaintextPasswordFetched(index,
                                       base::UTF16ToUTF8(password_value));
  }
}

void PasswordsPrivateDelegateImpl::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  // Create a list of PasswordUiEntry objects to send to observers.
  current_entries_.clear();

  for (size_t i = 0; i < password_list.size(); i++) {
    const auto& form = password_list[i];
    api::passwords_private::UrlCollection urls =
        CreateUrlCollectionFromForm(*form);

    api::passwords_private::PasswordUiEntry entry;
    entry.login_pair.urls = std::move(urls);
    entry.login_pair.username = base::UTF16ToUTF8(form->username_value);
    entry.index = base::checked_cast<int>(i);
    entry.num_characters_in_password = form->password_value.length();

    if (!form->federation_origin.unique()) {
      entry.federation_text.reset(new std::string(l10n_util::GetStringFUTF8(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form->federation_origin.host()))));
    }

    current_entries_.push_back(std::move(entry));
  }

  SendSavedPasswordsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_entries_initialized_ = true;
  InitializeIfNecessary();

  for (const auto& callback : get_saved_passwords_list_callbacks_)
    callback.Run(current_entries_);
  get_saved_passwords_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  // Creates a list of exceptions to send to observers.
  current_exceptions_.clear();

  for (size_t i = 0; i < password_exception_list.size(); i++) {
    const auto& form = password_exception_list[i];
    api::passwords_private::UrlCollection urls =
        CreateUrlCollectionFromForm(*form);

    api::passwords_private::ExceptionEntry current_exception_entry;
    current_exception_entry.urls = std::move(urls);
    current_exception_entry.index = base::checked_cast<int>(i);
    current_exceptions_.push_back(std::move(current_exception_entry));
  }

  SendPasswordExceptionsList();

  DCHECK(!current_entries_initialized_ ||
         get_saved_passwords_list_callbacks_.empty());

  current_exceptions_initialized_ = true;
  InitializeIfNecessary();

  for (const auto& callback : get_password_exception_list_callbacks_)
    callback.Run(current_exceptions_);
  get_password_exception_list_callbacks_.clear();
}

void PasswordsPrivateDelegateImpl::ImportPasswords(
    content::WebContents* web_contents) {
  password_manager_presenter_->ImportPasswords(web_contents);
}

void PasswordsPrivateDelegateImpl::ExportPasswords(
    content::WebContents* web_contents) {
  password_manager_presenter_->ExportPasswords(web_contents);
}

#if !defined(OS_ANDROID)
gfx::NativeWindow PasswordsPrivateDelegateImpl::GetNativeWindow() const {
  DCHECK(web_contents_);
  return web_contents_->GetTopLevelNativeWindow();
}
#endif

void PasswordsPrivateDelegateImpl::Shutdown() {
  password_manager_presenter_.reset();
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(
    const base::Closure& callback) {
  if (is_initialized_) {
    callback.Run();
    return;
  }

  pre_initialization_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ || !current_entries_initialized_ ||
      !current_exceptions_initialized_)
    return;

  is_initialized_ = true;

  for (const base::Closure& callback : pre_initialization_callbacks_)
    callback.Run();
  pre_initialization_callbacks_.clear();
}

}  // namespace extensions
