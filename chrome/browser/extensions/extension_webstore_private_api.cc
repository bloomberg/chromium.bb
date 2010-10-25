// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"

namespace {

const char* install_base_url = extension_urls::kGalleryUpdateHttpsUrl;
const char kAlreadyLoggedInError[] = "User already logged in";
const char kLoginKey[] = "login";
const char kTokenKey[] = "token";

ProfileSyncService* test_sync_service = NULL;
BrowserSignin* test_signin = NULL;

// Returns either the test sync service, or the real one from |profile|.
ProfileSyncService* GetSyncService(Profile* profile) {
  if (test_sync_service)
    return test_sync_service;
  else
    return profile->GetProfileSyncService();
}

BrowserSignin* GetBrowserSignin(Profile* profile) {
  if (test_signin)
    return test_signin;
  else
    return profile->GetBrowserSignin();
}

bool IsWebStoreURL(Profile* profile, const GURL& url) {
  ExtensionsService* service = profile->GetExtensionsService();
  Extension* store = service->GetWebStoreApp();
  DCHECK(store);
  return (service->GetExtensionByWebExtent(url) == store);
}

}  // namespace

// static
void WebstorePrivateApi::SetTestingProfileSyncService(
    ProfileSyncService* service) {
  test_sync_service = service;
}

// static
void WebstorePrivateApi::SetTestingBrowserSignin(BrowserSignin* signin) {
  test_signin = signin;
}

// static
void InstallFunction::SetTestingInstallBaseUrl(
    const char* testing_install_base_url) {
  install_base_url = testing_install_base_url;
}

bool InstallFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  std::string id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id));
  EXTENSION_FUNCTION_VALIDATE(Extension::IdIsValid(id));

  std::vector<std::string> params;
  params.push_back("id=" + id);
  params.push_back("lang=" + g_browser_process->GetApplicationLocale());
  params.push_back("uc");
  std::string url_string = install_base_url;

  GURL url(url_string + "?response=redirect&x=" +
      EscapeQueryParamValue(JoinString(params, '&'), true));
  DCHECK(url.is_valid());

  // Cleared in ~CrxInstaller().
  CrxInstaller::SetWhitelistedInstallId(id);

  // The download url for the given |id| is now contained in |url|. We
  // navigate the current (calling) tab to this url which will result in a
  // download starting. Once completed it will go through the normal extension
  // install flow. The above call to SetWhitelistedInstallId will bypass the
  // normal permissions install dialog.
  NavigationController& controller =
      dispatcher()->delegate()->associated_tab_contents()->controller();
  controller.LoadURL(url, source_url(), PageTransition::LINK);

  return true;
}

bool GetBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  std::string username = GetBrowserSignin(profile_)->GetSignedInUsername();
  DictionaryValue* dictionary = new DictionaryValue();
  dictionary->SetString(kLoginKey, username);
  // TODO(asargent) - send the browser login token here too if available.
  result_.reset(dictionary);
  return true;
}

bool GetStoreLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionPrefs* prefs = service->extension_prefs();
  std::string login;
  if (prefs->GetWebStoreLogin(&login)) {
    result_.reset(Value::CreateStringValue(login));
  } else {
    result_.reset(Value::CreateStringValue(std::string()));
  }
  return true;
}

bool SetStoreLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;
  std::string login;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &login));
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionPrefs* prefs = service->extension_prefs();
  prefs->SetWebStoreLogin(login);
  return true;
}

PromptBrowserLoginFunction::~PromptBrowserLoginFunction() {
}

bool PromptBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  // Login can currently only be invoked tab-modal.  Since this is
  // coming from the webstore, we should always have a tab, but check
  // just in case.
  TabContents* tab = dispatcher()->delegate()->associated_tab_contents();
  if (!tab)
    return false;

  std::string username = GetBrowserSignin(profile_)->GetSignedInUsername();
  std::string preferred_email;
  if (args_->GetSize() > 0) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &preferred_email));
    if (!username.empty()) {
      error_ = kAlreadyLoggedInError;
      return false;
    }
  }

  // We return the result asynchronously, so we addref to keep ourself alive.
  // Matched with a Release in OnLoginSuccess() and OnLoginFailure().
  AddRef();

  // TODO(johnnyg): Hook up preferred_email.
  GetBrowserSignin(profile_)->RequestSignin(tab, GetLoginMessage(), this);

  // The response will be sent asynchronously in OnLoginSuccess/OnLoginFailure.
  return true;
}

string16 PromptBrowserLoginFunction::GetLoginMessage() {
  using l10n_util::GetStringUTF16;
  using l10n_util::GetStringFUTF16;

  // TODO(johnnyg): This would be cleaner as an HTML template.
  // http://crbug.com/60216
  string16 message;
  message = ASCIIToUTF16("<p>")
      + GetStringUTF16(IDS_WEB_STORE_LOGIN_INTRODUCTION_1)
      + ASCIIToUTF16("</p>");
  message = message + ASCIIToUTF16("<p>")
      + GetStringFUTF16(IDS_WEB_STORE_LOGIN_INTRODUCTION_2,
                        GetStringUTF16(IDS_PRODUCT_NAME))
      + ASCIIToUTF16("</p>");
  return message;
}

void PromptBrowserLoginFunction::OnLoginSuccess() {
  // TODO(asargent) - send the browser login token here too if available.
  DictionaryValue* dictionary = new DictionaryValue();
  std::string username = GetBrowserSignin(profile_)->GetSignedInUsername();
  dictionary->SetString(kLoginKey, username);

  result_.reset(dictionary);
  SendResponse(true);

  // Ensure that apps are synced.
  // - If the user has already setup sync, we add Apps to the current types.
  // - If not, we create a new set which is just Apps.
  ProfileSyncService* service = GetSyncService(profile_);
  syncable::ModelTypeSet types;
  if (service->HasSyncSetupCompleted())
    service->GetPreferredDataTypes(&types);
  types.insert(syncable::APPS);
  service->ChangePreferredDataTypes(types);
  service->SetSyncSetupCompleted();

  // Matches the AddRef in RunImpl().
  Release();
}

void PromptBrowserLoginFunction::OnLoginFailure(
    const GoogleServiceAuthError& error) {
  SendResponse(false);
  // Matches the AddRef in RunImpl().
  Release();
}
