// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

std::ostream& operator<<(std::ostream& stream, ExecutionMode mode) {
  switch (mode) {
    case ExecutionMode::kNone:
      stream << "ExecutionModeNone";
      break;
    case ExecutionMode::kScanning:
      stream << "ExecutionModeScanning";
      break;
    case ExecutionMode::kCleanup:
      stream << "ExecutionModeCleanup";
      break;
    case ExecutionMode::kNumValues:
      stream << "ExecutionModeNumValues";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, ChromePromptValue value) {
  switch (value) {
    case ChromePromptValue::kUnspecified:
      stream << "ChromePromptUnspecified";
      break;
    case ChromePromptValue::kPrompted:
      stream << "ChromePromptPrompted";
      break;
    case ChromePromptValue::kUserInitiated:
      stream << "ChromePromptUserInitiated";
      break;
    case ChromePromptValue::kLegacyNotPrompted:
      stream << "ChromePromptLegacyNotPrompted";
      break;
    case ChromePromptValue::kLegacyUnknown:
      stream << "ChromePromptLegacyUnknown";
      break;
    case ChromePromptValue::kLegacyShownFromMenu:
      stream << "ChromePromptLegacyShownFromMenu";
      break;
  }
  return stream;
}

}  // namespace chrome_cleaner
