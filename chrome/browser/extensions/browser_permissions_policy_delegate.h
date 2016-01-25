// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_PERMISSIONS_POLICY_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_PERMISSIONS_POLICY_DELEGATE_H_

#include "base/macros.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

// Policy delegate for the browser process.
class BrowserPermissionsPolicyDelegate
    : public PermissionsData::PolicyDelegate {
 public:
  BrowserPermissionsPolicyDelegate();
  ~BrowserPermissionsPolicyDelegate() override;

  bool CanExecuteScriptOnPage(const Extension* extension,
                              const GURL& document_url,
                              int tab_id,
                              std::string* error) override;

  DISALLOW_COPY_AND_ASSIGN(BrowserPermissionsPolicyDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_PERMISSIONS_POLICY_DELEGATE_H_
