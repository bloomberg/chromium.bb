// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/settings_private.h"

class PrefService;
class Profile;

namespace extensions {

namespace prefs_util {

using TypedPrefMap = std::map<std::string, api::settings_private::PrefType>;

// Gets the list of whitelisted pref keys -- that is, those which correspond to
// prefs that clients of the settingsPrivate API may retrieve and manipulate.
const TypedPrefMap& GetWhitelistedKeys();

// Gets the value of the pref with the given |name|. Returns a pointer to an
// empty PrefObject if no pref is found for |name|.
scoped_ptr<api::settings_private::PrefObject> GetPref(Profile* profile,
                                                      const std::string& name);

// Returns whether |pref_name| corresponds to a pref whose type is URL.
bool IsPrefTypeURL(const std::string& pref_name);

// Returns whether |pref_name| corresponds to a pref that is user modifiable
// (i.e., not made restricted by a user or device policy).
bool IsPrefUserModifiable(Profile* profile, const std::string& pref_name);

// Returns a pointer to the appropriate PrefService instance for the given
// |pref_name|.
PrefService* FindServiceForPref(Profile* profile, const std::string& pref_name);

}  // namespace prefs_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_
