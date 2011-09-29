// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_RULE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_RULE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"

namespace content_settings {

struct Rule {
  Rule();
  Rule(const ContentSettingsPattern& primary_pattern,
       const ContentSettingsPattern& secondary_pattern,
       ContentSetting setting);

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  ContentSetting content_setting;
};

class RuleIterator {
 public:
  virtual ~RuleIterator();
  virtual bool HasNext() const = 0;
  virtual Rule Next() = 0;
};

class EmptyRuleIterator : public RuleIterator {
 public:
  virtual ~EmptyRuleIterator();
  virtual bool HasNext() const OVERRIDE;
  virtual Rule Next() OVERRIDE;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_RULE_H_
