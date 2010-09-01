// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webstore_private_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/sync/profile_sync_service.h"

static bool IsWebStoreURL(const GURL& url) {
  GURL store_url(Extension::ChromeStoreURL());
  if (!url.is_valid() || !store_url.is_valid()) {
    return false;
  }
  return store_url.GetOrigin() == url.GetOrigin();
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
