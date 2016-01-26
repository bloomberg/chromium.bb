// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplayOmitScheme| based on origin of |password_form|
// and |languages|) and without prefixes "m.", "mobile." or "www.". For Android
// URIs, returns the result of GetHumanReadableOriginForAndroidUri and sets
// |*is_android_uri| to true, otherwise |*is_android_uri| is set to false.
// |is_android_uri| is required to be non-null.
std::string GetShownOrigin(const autofill::PasswordForm& password_form,
                           const std::string& languages,
                           bool* is_android_uri);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplayOmitScheme| based on origin of |password_form|
// and |languages|) and without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const GURL& origin, const std::string& languages);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
