// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/content_settings/content_settings_rule.h"

namespace content_settings {

Rule::Rule()
    : content_setting(CONTENT_SETTING_DEFAULT) { }

Rule::Rule(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      content_setting(setting) {
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
}

RuleIterator::~RuleIterator() {}

EmptyRuleIterator::~EmptyRuleIterator() {}

bool EmptyRuleIterator::HasNext() const {
  return false;
}

Rule EmptyRuleIterator::Next() {
  NOTREACHED();
  return Rule();
}

}  // namespace content_settings
