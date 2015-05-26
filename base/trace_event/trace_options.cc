// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_options.h"

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace base {
namespace trace_event {

namespace {

// String options that can be used to initialize TraceOptions.
const char kRecordUntilFull[] = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kRecordAsMuchAsPossible[] = "record-as-much-as-possible";
const char kTraceToConsole[] = "trace-to-console";
const char kEnableSampling[] = "enable-sampling";
const char kEnableSystrace[] = "enable-systrace";
const char kEnableArgumentFilter[] = "enable-argument-filter";

}  // namespace

bool TraceOptions::SetFromString(const std::string& options_string) {
  record_mode = RECORD_UNTIL_FULL;
  enable_sampling = false;
  enable_systrace = false;

  std::vector<std::string> split;
  std::vector<std::string>::iterator iter;
  base::SplitString(options_string, ',', &split);
  for (iter = split.begin(); iter != split.end(); ++iter) {
    if (*iter == kRecordUntilFull) {
      record_mode = RECORD_UNTIL_FULL;
    } else if (*iter == kRecordContinuously) {
      record_mode = RECORD_CONTINUOUSLY;
    } else if (*iter == kTraceToConsole) {
      record_mode = ECHO_TO_CONSOLE;
    } else if (*iter == kRecordAsMuchAsPossible) {
      record_mode = RECORD_AS_MUCH_AS_POSSIBLE;
    } else if (*iter == kEnableSampling) {
      enable_sampling = true;
    } else if (*iter == kEnableSystrace) {
      enable_systrace = true;
    } else if (*iter == kEnableArgumentFilter) {
      enable_argument_filter = true;
    } else {
      return false;
    }
  }
  return true;
}

std::string TraceOptions::ToString() const {
  std::string ret;
  switch (record_mode) {
    case RECORD_UNTIL_FULL:
      ret = kRecordUntilFull;
      break;
    case RECORD_CONTINUOUSLY:
      ret = kRecordContinuously;
      break;
    case ECHO_TO_CONSOLE:
      ret = kTraceToConsole;
      break;
    case RECORD_AS_MUCH_AS_POSSIBLE:
      ret = kRecordAsMuchAsPossible;
      break;
    default:
      NOTREACHED();
  }
  if (enable_sampling)
    ret = ret + "," + kEnableSampling;
  if (enable_systrace)
    ret = ret + "," + kEnableSystrace;
  if (enable_argument_filter)
    ret = ret + "," + kEnableArgumentFilter;
  return ret;
}

}  // namespace trace_event
}  // namespace base
