// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Content Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_CONSTANTS_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_CONSTANTS_H__
#pragma once

namespace extension_content_settings_api_constants {

// Keys.
extern const char kContentSettingKey[];
extern const char kContentSettingsTypeKey[];
extern const char kDescriptionKey[];
extern const char kEmbeddedPatternKey[];
extern const char kEmbeddedUrlKey[];
extern const char kIdKey[];
extern const char kIncognitoKey[];
extern const char kOnlyNonDefaultKey[];
extern const char kResourceIdentifierKey[];
extern const char kRuleKey[];
extern const char kTopLevelPatternKey[];
extern const char kUrlKey[];

// Errors.
extern const char kInvalidContentSettingError[];
extern const char kInvalidUrlError[];

}

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTENT_SETTINGS_API_CONSTANTS_H__
