// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_
#define CHROME_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/common/content_settings_pattern.h"

namespace content_settings {

class PatternParser {
 public:
  static void Parse(const std::string& pattern_spec,
                    ContentSettingsPattern::BuilderInterface* builder);

  static std::string ToString(
      const ContentSettingsPattern::PatternParts& parts);
};

}  // namespace content_settings

#endif  // CHROME_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_
