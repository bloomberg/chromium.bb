// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/expectations/expectation.h"

namespace test_expectations {

bool ResultFromString(const std::string& result, Result* out_result) {
  if (result == "Failure")
    *out_result = RESULT_FAILURE;
  else if (result == "Timeout")
    *out_result = RESULT_TIMEOUT;
  else if (result == "Crash")
    *out_result = RESULT_CRASH;
  else if (result == "Skip")
    *out_result = RESULT_SKIP;
  else if (result == "Pass")
    *out_result = RESULT_PASS;
  else
    return false;

  return true;
}

static bool IsValidPlatform(const Platform* platform) {
  const std::string& name = platform->name;
  const std::string& variant = platform->variant;

  if (name == "Win") {
    if (variant != "" &&
        variant != "XP" &&
        variant != "Vista" &&
        variant != "7" &&
        variant != "8") {
      return false;
    }
  } else if (name == "Mac") {
    if (variant != "" &&
        variant != "10.6" &&
        variant != "10.7" &&
        variant != "10.8") {
      return false;
    }
  } else if (name == "Linux") {
    if (variant != "" &&
        variant != "x32" &&
        variant != "x64") {
      return false;
    }
  } else if (name == "ChromeOS") {
    // TODO(rsesek): Figure out what ChromeOS needs.
  } else if (name == "iOS") {
    // TODO(rsesek): Figure out what iOS needs. Probably Device and Simulator.
  } else if (name == "Android") {
    // TODO(rsesek): Figure out what Android needs.
  } else {
    return false;
  }

  return true;
}

bool PlatformFromString(const std::string& modifier, Platform* out_platform) {
  size_t sep = modifier.find('-');
  if (sep == std::string::npos) {
    out_platform->name = modifier;
    out_platform->variant.clear();
  } else {
    out_platform->name = modifier.substr(0, sep);
    out_platform->variant = modifier.substr(sep + 1);
  }

  return IsValidPlatform(out_platform);
}

bool ConfigurationFromString(const std::string& modifier,
                             Configuration* out_configuration) {
  if (modifier == "Debug")
    *out_configuration = CONFIGURATION_DEBUG;
  else if (modifier == "Release")
    *out_configuration = CONFIGURATION_RELEASE;
  else
    return false;

  return true;
}

Expectation::Expectation()
    : configuration(CONFIGURATION_UNSPECIFIED),
      result(RESULT_PASS) {
}

Expectation::~Expectation() {}

}  // namespace test_expectations
