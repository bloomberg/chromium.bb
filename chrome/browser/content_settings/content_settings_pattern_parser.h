// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_PARSER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"

namespace content_settings {

struct PatternParts;

class PatternParser {
 public:
  static void Parse(const std::string& pattern_spec,
                    ContentSettingsPattern::BuilderInterface* builder);

  static std::string ToString(
      const ContentSettingsPattern::PatternParts& parts);

 private:
  static const char* kDomainWildcard;

  static const size_t kDomainWildcardLength;

  static const char* kSchemeWildcard;

  static const char* kHostWildcard;

  static const char* kPortWildcard;

  DISALLOW_COPY_AND_ASSIGN(PatternParser);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_PARSER_H_
