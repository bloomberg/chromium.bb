// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/crash_logging.h"

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

namespace base {
namespace debug {

static SetCrashKeyValueFuncT g_set_key_func_ = NULL;
static ClearCrashKeyValueFuncT g_clear_key_func_ = NULL;

void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  if (g_set_key_func_)
    g_set_key_func_(key, value);
}

void ClearCrashKey(const base::StringPiece& key) {
  if (g_clear_key_func_)
    g_clear_key_func_(key);
}

void SetCrashKeyToStackTrace(const base::StringPiece& key,
                             const StackTrace& trace) {
  size_t count = 0;
  const void* const* addresses = trace.Addresses(&count);
  SetCrashKeyFromAddresses(key, addresses, count);
}

void SetCrashKeyFromAddresses(const base::StringPiece& key,
                              const void* const* addresses,
                              size_t count) {
  std::string value = "<null>";
  if (addresses && count) {
    const size_t kBreakpadValueMax = 255;

    std::vector<std::string> hex_backtrace;
    size_t length = 0;

    for (size_t i = 0; i < count; ++i) {
      std::string s = base::StringPrintf("%p", addresses[i]);
      length += s.length() + 1;
      if (length > kBreakpadValueMax)
        break;
      hex_backtrace.push_back(s);
    }

    value = JoinString(hex_backtrace, ' ');

    // Warn if this exceeds the breakpad limits.
    DCHECK_LE(value.length(), kBreakpadValueMax);
  }

  SetCrashKeyValue(key, value);
}

ScopedCrashKey::ScopedCrashKey(const base::StringPiece& key,
                               const base::StringPiece& value)
    : key_(key.as_string()) {
  SetCrashKeyValue(key, value);
}

ScopedCrashKey::~ScopedCrashKey() {
  ClearCrashKey(key_);
}

void SetCrashKeyReportingFunctions(
    SetCrashKeyValueFuncT set_key_func,
    ClearCrashKeyValueFuncT clear_key_func) {
  g_set_key_func_ = set_key_func;
  g_clear_key_func_ = clear_key_func;
}

}  // namespace debug
}  // namespace base
