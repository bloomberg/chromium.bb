// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_api_constants.h"

namespace extension_content_settings_api_constants {

// Keys.
const char kContentSettingKey[] = "setting";
const char kContentSettingsTypeKey[] = "type";
const char kDescriptionKey[] = "description";
const char kEmbeddedPatternKey[] = "embeddedPattern";
const char kEmbeddedUrlKey[] = "embeddedUrl";
const char kIdKey[] = "id";
const char kPatternKey[] = "pattern";
const char kResourceIdentifierKey[] = "resourceIdentifier";
const char kRuleKey[] = "rule";
const char kTopLevelPatternKey[] = "topLevelPattern";
const char kTopLevelUrlKey[] = "topLevelUrl";

// Errors.
const char kIncognitoContextError[] =
    "Can't modify regular settings from an incognito context.";
const char kIncognitoSessionOnlyError[] =
    "You cannot read incognito content settings when no incognito window "
    "is open.";
const char kInvalidUrlError[] = "The URL \"*\" is invalid.";

}  // extension_content_settings_api_constants
