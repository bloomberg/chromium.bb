// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_handler.h"

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/password_form.h"

PasswordManagerHandler::PasswordManagerHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(populater_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(exception_populater_(this)) {
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
    { "passwordsSiteColumn",
      IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN },
    { "passwordsUsernameColumn",
      IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN },
    { "passwordsRemoveButton",
      IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON },
    { "passwordsNoPasswordsDescription",
     IDS_PASSWORDS_PAGE_VIEW_NO_PASSWORDS_DESCRIPTION },
    { "passwordsNoExceptionsDescription",
     IDS_PASSWORDS_PAGE_VIEW_NO_EXCEPTIONS_DESCRIPTION },
    { "passwordsRemoveAllButton",
      IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON },
    { "passwordsRemoveAllTitle",
      IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS },
    { "passwordsRemoveAllWarning",
      IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "passwordsPage",
                IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE);

  localized_strings->SetString("passwordManagerLearnMoreURL",
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kPasswordManagerLearnMoreURL)).spec());
}

void PasswordManagerHandler::Initialize() {
  // Due to the way that handlers are (re)initialized under certain types of
  // navigation, we may already be initialized. (See bugs 88986 and 86448.)
  // If this is the case, return immediately. This is a hack.
  // TODO(mdm): remove this hack once it is no longer necessary.
  if (!show_passwords_.GetPrefName().empty())
    return;

  show_passwords_.Init(prefs::kPasswordManagerAllowShowPasswords,
                       Profile::FromWebUI(web_ui_)->GetPrefs(), this);
  // We should not cache web_ui_->GetProfile(). See crosbug.com/6304.
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->AddObserver(this);
}

void PasswordManagerHandler::RegisterMessages() {
  DCHECK(web_ui_);

  web_ui_->RegisterMessageCallback("updatePasswordLists",
      base::Bind(&PasswordManagerHandler::UpdatePasswordLists,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeSavedPassword",
      base::Bind(&PasswordManagerHandler::RemoveSavedPassword,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removePasswordException",
      base::Bind(&PasswordManagerHandler::RemovePasswordException,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeAllSavedPasswords",
      base::Bind(&PasswordManagerHandler::RemoveAllSavedPasswords,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeAllPasswordExceptions",
      base::Bind(&PasswordManagerHandler::RemoveAllPasswordExceptions,
                 base::Unretained(this)));
}

void PasswordManagerHandler::OnLoginsChanged() {
  UpdatePasswordLists(NULL);
}

PasswordStore* PasswordManagerHandler::GetPasswordStore() {
  return Profile::FromWebUI(web_ui_)->
      GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordManagerHandler::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = Details<std::string>(details).ptr();
    if (*pref_name == prefs::kPasswordManagerAllowShowPasswords) {
      UpdatePasswordLists(NULL);
    }
  }

  OptionsPageUIHandler::Observe(type, source, details);
}

void PasswordManagerHandler::UpdatePasswordLists(const ListValue* args) {
  // Reset the current lists.
  password_list_.reset();
  password_exception_list_.reset();

  languages_ = Profile::FromWebUI(web_ui_)->GetPrefs()->
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
    Initialize();

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

  web_ui_->CallJavascriptFunction("PasswordManager.setSavedPasswordsList",
                                  entries);
}

void PasswordManagerHandler::SetPasswordExceptionList() {
  ListValue entries;
  for (size_t i = 0; i < password_exception_list_.size(); ++i) {
    entries.Append(new StringValue(
        net::FormatUrl(password_exception_list_[i]->origin, languages_)));
  }

  web_ui_->CallJavascriptFunction("PasswordManager.setPasswordExceptionsList",
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
        const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_list_.reset();
  page_->password_list_.insert(page_->password_list_.end(),
                               result.begin(), result.end());
  page_->SetPasswordList();
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
        const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_exception_list_.reset();
  page_->password_exception_list_.insert(page_->password_exception_list_.end(),
                                         result.begin(), result.end());
  page_->SetPasswordExceptionList();
}
