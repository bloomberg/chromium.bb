// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"

#include "base/strings/string_number_conversions.h"

namespace subresource_filter {

bool ParseFlag(const std::string& text,
               RulesetFormat* format,
               std::string* error) {
  if (text.empty()) {
    *format = RulesetFormat::kUndefined;
    return true;
  }
  if (text == "filter-list") {
    *format = RulesetFormat::kFilterList;
    return true;
  }
  if (text == "proto") {
    *format = RulesetFormat::kProto;
    return true;
  }
  if (text == "unindexed-ruleset") {
    *format = RulesetFormat::kUnindexedRuleset;
    return true;
  }
  *error = "unknown RulesetFormat";
  return false;
}

std::string UnparseFlag(RulesetFormat format) {
  switch (format) {
    case RulesetFormat::kUndefined:
      return "";
    case RulesetFormat::kFilterList:
      return "filter-list";
    case RulesetFormat::kProto:
      return "proto";
    case RulesetFormat::kUnindexedRuleset:
      return "unindexed-ruleset";
    default:
      return base::IntToString(static_cast<int>(format));
  }
}

}  // namespace subresource_filter
