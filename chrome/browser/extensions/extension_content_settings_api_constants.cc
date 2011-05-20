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
const char kIncognitoKey[] = "incognito";
const char kOnlyNonDefaultKey[] = "onlyNonDefault";
const char kResourceIdentifierKey[] = "resourceIdentifier";
const char kRuleKey[] = "rule";
const char kTopLevelPatternKey[] = "topLevelPattern";
const char kUrlKey[] = "url";

// Errors.
const char kInvalidContentSettingError[] =
    "Invalid content setting \"*\" for content type \"*\".";
const char kInvalidUrlError[] = "The URL \"*\" is invalid.";

}  // extension_content_settings_api_constants
