// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/password_form.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

PasswordManagerHandler::PasswordManagerHandler()
    : populater_(this),
      exception_populater_(this) {
}

PasswordManagerHandler::~PasswordManagerHandler() {
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->RemoveObserver(this);
}

void PasswordManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
    { "savedPasswordsTitle",
      IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE },
    { "passwordExceptionsTitle",
      IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE },
    { "passwordSearchPlaceholder",
      IDS_PASSWORDS_PAGE_SEARCH_PASSWORDS },
    { "passwordShowButton",
      IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON },
    { "passwordHideButton",
      IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON },
    { "passwordsNoPasswordsDescription",
      IDS_PASSWORDS_PAGE_VIEW_NO_PASSWORDS_DESCRIPTION },
    { "passwordsNoExceptionsDescription",
      IDS_PASSWORDS_PAGE_VIEW_NO_EXCEPTIONS_DESCRIPTION },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "passwordsPage",
                IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);

  localized_strings->SetString("passwordManagerLearnMoreURL",
                               chrome::kPasswordManagerLearnMoreURL);
}

void PasswordManagerHandler::InitializeHandler() {
  // Due to the way that handlers are (re)initialized under certain types of
  // navigation, we may already be initialized. (See bugs 88986 and 86448.)
  // If this is the case, return immediately. This is a hack.
  // TODO(mdm): remove this hack once it is no longer necessary.
  if (!show_passwords_.GetPrefName().empty())
    return;

  show_passwords_.Init(prefs::kPasswordManagerAllowShowPasswords,
                       Profile::FromWebUI(web_ui())->GetPrefs(),
                       base::Bind(&PasswordManagerHandler::UpdatePasswordLists,
                                  base::Unretained(this),
                                  static_cast<base::ListValue*>(NULL)));
  // We should not cache web_ui()->GetProfile(). See crosbug.com/6304.
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->AddObserver(this);
}

void PasswordManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("updatePasswordLists",
      base::Bind(&PasswordManagerHandler::UpdatePasswordLists,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeSavedPassword",
      base::Bind(&PasswordManagerHandler::RemoveSavedPassword,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removePasswordException",
      base::Bind(&PasswordManagerHandler::RemovePasswordException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeAllSavedPasswords",
      base::Bind(&PasswordManagerHandler::RemoveAllSavedPasswords,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeAllPasswordExceptions",
      base::Bind(&PasswordManagerHandler::RemoveAllPasswordExceptions,
                 base::Unretained(this)));
}

void PasswordManagerHandler::OnLoginsChanged() {
  UpdatePasswordLists(NULL);
}

PasswordStore* PasswordManagerHandler::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                             Profile::EXPLICIT_ACCESS);
}

void PasswordManagerHandler::UpdatePasswordLists(const ListValue* args) {
  // Reset the current lists.
  password_list_.clear();
  password_exception_list_.clear();

  languages_ = Profile::FromWebUI(web_ui())->GetPrefs()->
      GetString(prefs::kAcceptLanguages);
  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordManagerHandler::RemoveSavedPassword(const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  std::string string_value = UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0 &&
      static_cast<size_t>(index) < password_list_.size())
    store->RemoveLogin(*password_list_[index]);
}

void PasswordManagerHandler::RemovePasswordException(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  std::string string_value = UTF16ToUTF8(ExtractStringValue(args));
  int index;
  if (base::StringToInt(string_value, &index) && index >= 0 &&
      static_cast<size_t>(index) < password_exception_list_.size())
    store->RemoveLogin(*password_exception_list_[index]);
}

void PasswordManagerHandler::RemoveAllSavedPasswords(
    const ListValue* args) {
  // TODO(jhawkins): This will cause a list refresh for every password in the
  // list. Add PasswordStore::RemoveAllLogins().
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  for (size_t i = 0; i < password_list_.size(); ++i)
    store->RemoveLogin(*password_list_[i]);
}

void PasswordManagerHandler::RemoveAllPasswordExceptions(
    const ListValue* args) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  for (size_t i = 0; i < password_exception_list_.size(); ++i)
    store->RemoveLogin(*password_exception_list_[i]);
}

void PasswordManagerHandler::SetPasswordList() {
  // Due to the way that handlers are (re)initialized under certain types of
  // navigation, we may not be initialized yet. (See bugs 88986 and 86448.)
  // If this is the case, initialize on demand. This is a hack.
  // TODO(mdm): remove this hack once it is no longer necessary.
  if (show_passwords_.GetPrefName().empty())
    InitializeHandler();

  ListValue entries;
  bool show_passwords = *show_passwords_;
  string16 empty;
  for (size_t i = 0; i < password_list_.size(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue(net::FormatUrl(password_list_[i]->origin,
                                                 languages_)));
    entry->Append(new StringValue(password_list_[i]->username_value));
    entry->Append(new StringValue(
        show_passwords ? password_list_[i]->password_value : empty));
    entries.Append(entry);
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setSavedPasswordsList",
                                   entries);
}

void PasswordManagerHandler::SetPasswordExceptionList() {
  ListValue entries;
  for (size_t i = 0; i < password_exception_list_.size(); ++i) {
    entries.Append(new StringValue(
        net::FormatUrl(password_exception_list_[i]->origin, languages_)));
  }

  web_ui()->CallJavascriptFunction("PasswordManager.setPasswordExceptionsList",
                                   entries);
}

PasswordManagerHandler::ListPopulater::ListPopulater(
    PasswordManagerHandler* page)
    : page_(page),
      pending_login_query_(0) {
}

PasswordManagerHandler::ListPopulater::~ListPopulater() {
}

PasswordManagerHandler::PasswordListPopulater::PasswordListPopulater(
    PasswordManagerHandler* page) : ListPopulater(page) {
}

void PasswordManagerHandler::PasswordListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    if (pending_login_query_)
      store->CancelRequest(pending_login_query_);

    pending_login_query_ = store->GetAutofillableLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display passwords.";
  }
}

void PasswordManagerHandler::PasswordListPopulater::
    OnPasswordStoreRequestDone(
        CancelableRequestProvider::Handle handle,
        const std::vector<content::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_list_.clear();
  page_->password_list_.insert(page_->password_list_.end(),
                               result.begin(), result.end());
  page_->SetPasswordList();
}

void PasswordManagerHandler::PasswordListPopulater::OnGetPasswordStoreResults(
    const std::vector<content::PasswordForm*>& results) {
  // TODO(kaiwang): Implement when I refactor
  // PasswordStore::GetAutofillableLogins and PasswordStore::GetBlacklistLogins.
  NOTIMPLEMENTED();
}

PasswordManagerHandler::PasswordExceptionListPopulater::
    PasswordExceptionListPopulater(PasswordManagerHandler* page)
        : ListPopulater(page) {
}

void PasswordManagerHandler::PasswordExceptionListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    if (pending_login_query_)
      store->CancelRequest(pending_login_query_);

    pending_login_query_ = store->GetBlacklistLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display exceptions.";
  }
}

void PasswordManagerHandler::PasswordExceptionListPopulater::
    OnPasswordStoreRequestDone(
        CancelableRequestProvider::Handle handle,
        const std::vector<content::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_exception_list_.clear();
  page_->password_exception_list_.insert(page_->password_exception_list_.end(),
                                         result.begin(), result.end());
  page_->SetPasswordExceptionList();
}

void PasswordManagerHandler::PasswordExceptionListPopulater::
    OnGetPasswordStoreResults(
        const std::vector<content::PasswordForm*>& results) {
  // TODO(kaiwang): Implement when I refactor
  // PasswordStore::GetAutofillableLogins and PasswordStore::GetBlacklistLogins.
  NOTIMPLEMENTED();
}

}  // namespace options
