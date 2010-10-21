// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension_constants.h"
#include "net/base/escape.h"

namespace {

const char* install_base_url = extension_urls::kGalleryUpdateHttpsUrl;
const char kAlreadyLoggedInError[] = "User already logged in";
const char kLoginKey[] = "login";
const char kTokenKey[] = "token";

ProfileSyncService* test_sync_service = NULL;

// Returns either the test sync service, or the real one from |profile|.
ProfileSyncService* GetSyncService(Profile* profile) {
  if (test_sync_service)
    return test_sync_service;
  else
    return profile->GetProfileSyncService();
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
  string16 username = GetSyncService(profile_)->GetAuthenticatedUsername();
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

PromptBrowserLoginFunction::~PromptBrowserLoginFunction() {}

bool PromptBrowserLoginFunction::RunImpl() {
  if (!IsWebStoreURL(profile_, source_url()))
    return false;

  std::string preferred_email;
  ProfileSyncService* sync_service = GetSyncService(profile_);
  if (args_->GetSize() > 0) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &preferred_email));
    if (!sync_service->GetAuthenticatedUsername().empty()) {
      error_ = kAlreadyLoggedInError;
      return false;
    }
  }

  // We return the result asynchronously, so we addref to keep ourself alive.
  // Matched with a Release in OnStateChanged().
  AddRef();

  sync_service->AddObserver(this);
  // TODO(mirandac/estade) - make use of |preferred_email| to pre-populate the
  // browser login dialog if it was set to non-empty above.
  sync_service->ShowLoginDialog(NULL);

  // The response will be sent asynchronously in OnStateChanged().
  return true;
}

void PromptBrowserLoginFunction::OnStateChanged() {
  ProfileSyncService* sync_service = GetSyncService(profile_);
  // If the setup is finished, we'll report back what happened.
  if (!sync_service->SetupInProgress()) {
    sync_service->RemoveObserver(this);
    DictionaryValue* dictionary = new DictionaryValue();

    // TODO(asargent) - send the browser login token here too if available.
    string16 username = sync_service->GetAuthenticatedUsername();
    dictionary->SetString(kLoginKey, username);

    result_.reset(dictionary);
    SendResponse(true);

    // Matches the AddRef in RunImpl().
    Release();
  }
}
