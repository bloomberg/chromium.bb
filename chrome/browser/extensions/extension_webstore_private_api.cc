// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

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

}

static bool IsWebStoreURL(const GURL& url) {
  GURL store_url(Extension::ChromeStoreURL());
  if (!url.is_valid() || !store_url.is_valid()) {
    return false;
  }

  // Ignore port. It may be set on |url| during tests, but cannot be set on
  // ChromeStoreURL.
  return store_url.scheme() == url.scheme() &&
         store_url.host() == url.host();
}

// static
void InstallFunction::SetTestingInstallBaseUrl(
    const char* testing_install_base_url) {
  install_base_url = testing_install_base_url;
}

bool InstallFunction::RunImpl() {
  if (!IsWebStoreURL(source_url()))
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

bool GetSyncLoginFunction::RunImpl() {
  if (!IsWebStoreURL(source_url()))
    return false;
  ProfileSyncService* sync_service = profile_->GetProfileSyncService();
  string16 username = sync_service->GetAuthenticatedUsername();
  result_.reset(Value::CreateStringValue(username));
  return true;
}

bool GetStoreLoginFunction::RunImpl() {
  if (!IsWebStoreURL(source_url()))
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
  if (!IsWebStoreURL(source_url()))
    return false;
  std::string login;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &login));
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionPrefs* prefs = service->extension_prefs();
  prefs->SetWebStoreLogin(login);
  return true;
}
