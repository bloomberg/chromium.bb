// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
#pragma once

#include <string>

#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/browser/extensions/extension_prefs_scope.h"

class Profile;

namespace base {
class ListValue;
};

namespace extension_preference_helpers {

bool StringToScope(const std::string& s, ExtensionPrefsScope* scope);

// Returns a string constant (defined in the API) indicating the level of
// control this extension has over the specified preference.
const char* GetLevelOfControl(
    Profile* profile,
    const std::string& extension_id,
    const std::string& browser_pref,
    bool incognito);

// Dispatches |event_name| to extensions listening to the event and holding
// |permission|. |args| is passed as arguments to the event listener.  A
// key-value for the level of control the extension has over |browser_pref| is
// injected into the first item of |args|, which must be of type
// DictionaryValue.
void DispatchEventToExtensions(
    Profile* profile,
    const std::string& event_name,
    base::ListValue* args,
    extensions::APIPermission::ID permission,
    bool incognito,
    const std::string& browser_pref);

} // namespace extension_preference_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
