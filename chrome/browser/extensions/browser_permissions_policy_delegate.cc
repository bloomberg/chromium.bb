// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_permissions_policy_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace errors = manifest_errors;

BrowserPermissionsPolicyDelegate::BrowserPermissionsPolicyDelegate() {
  PermissionsData::SetPolicyDelegate(this);
}
BrowserPermissionsPolicyDelegate::~BrowserPermissionsPolicyDelegate() {
  PermissionsData::SetPolicyDelegate(NULL);
}

bool BrowserPermissionsPolicyDelegate::CanExecuteScriptOnPage(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return true;
}

}  // namespace extensions
