// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/extension_token_key.h"

namespace extensions {

ExtensionTokenKey::ExtensionTokenKey(const std::string& extension_id,
                                     const std::string& account_id,
                                     const std::set<std::string>& scopes)
    : extension_id(extension_id), account_id(account_id), scopes(scopes) {}

ExtensionTokenKey::~ExtensionTokenKey() {}

bool ExtensionTokenKey::operator<(const ExtensionTokenKey& rhs) const {
  if (extension_id < rhs.extension_id)
    return true;
  else if (rhs.extension_id < extension_id)
    return false;

  if (account_id < rhs.account_id)
    return true;
  else if (rhs.account_id < account_id)
    return false;

  return scopes < rhs.scopes;
}

}  // namespace extensions
