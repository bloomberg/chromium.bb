// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ADMIN_POLICY_H_
#define EXTENSIONS_BROWSER_ADMIN_POLICY_H_

#include "base/values.h"

namespace extensions {

class Extension;

// Functions for providing information about the extension whitelist,
// blacklist, and forcelist imposed by admin policy.
namespace admin_policy {

// Checks if extensions are blacklisted by default, by policy. When true, this
// means that even extensions without an ID should be blacklisted (e.g.
// from the command line, or when loaded as an unpacked extension).
bool BlacklistedByDefault(const base::ListValue* blacklist);

// Returns true if the extension is allowed by the admin policy.
bool UserMayLoad(const base::ListValue* blacklist,
                 const base::ListValue* whitelist,
                 const base::DictionaryValue* forcelist,
                 const base::ListValue* allowed_types,
                 const Extension* extension,
                 base::string16* error);

// Returns false if the extension is required to remain running. In practice
// this enforces the admin policy forcelist.
bool UserMayModifySettings(const Extension* extension, base::string16* error);

// Returns false if the extension is required to remain running. In practice
// this enforces the admin policy forcelist.
bool MustRemainEnabled(const Extension* extension, base::string16* error);

}  // namespace admin_policy
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ADMIN_POLICY_H_
