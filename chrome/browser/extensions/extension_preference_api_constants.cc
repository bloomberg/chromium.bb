// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_preference_api_constants.h"

namespace extension_preference_api_constants {

const char kIncognitoKey[] = "incognito";

const char kScopeKey[] = "scope";

const char kNotControllable[] = "not_controllable";
const char kControlledByOtherExtensions[] = "controlled_by_other_extensions";
const char kControllableByThisExtension[] = "controllable_by_this_extension";
const char kControlledByThisExtension[] = "controlled_by_this_extension";

const char kIncognitoSpecific[] = "incognitoSpecific";
const char kLevelOfControl[] = "levelOfControl";
const char kValue[] = "value";

const char kIncognitoErrorMessage[] =
    "You do not have permission to access incognito preferences.";

const char kIncognitoSessionOnlyErrorMessage[] =
    "You cannot set a preference with scope 'incognito_session_only' when no "
    "incognito window is open.";

const char kPermissionErrorMessage[] =
    "You do not have permission to access the preference '*'. "
    "Be sure to declare in your manifest what permissions you need.";

}  // extension_preference_api_constants

