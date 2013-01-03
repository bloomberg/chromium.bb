// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_CRASH_LOGGING_H_
#define BASE_DEBUG_CRASH_LOGGING_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/string_piece.h"

// These functions add metadata to the upload payload when sending crash reports
// to the crash server.
//
// IMPORTANT: On OS X and Linux, the key/value pairs are only sent as part of
// the upload and are not included in the minidump!

namespace base {
namespace debug {

class StackTrace;

// Set or clear a specific key-value pair from the crash metadata.
BASE_EXPORT void SetCrashKeyValue(const base::StringPiece& key,
                                  const base::StringPiece& value);
BASE_EXPORT void ClearCrashKey(const base::StringPiece& key);

// Records the given StackTrace into a crash key.
BASE_EXPORT void SetCrashKeyToStackTrace(const base::StringPiece& key,
                                         const StackTrace& trace);

// Formats |count| instruction pointers from |addresses| using %p and
// sets the resulting string as a value for crash key |key. A maximum of 23
// items will be encoded, since breakpad limits values to 255 bytes.
BASE_EXPORT void SetCrashKeyFromAddresses(const base::StringPiece& key,
                                          const void* const* addresses,
                                          size_t count);

// A scoper that sets the specified key to value for the lifetime of the
// object, and clears it on destruction.
class BASE_EXPORT ScopedCrashKey {
 public:
  ScopedCrashKey(const base::StringPiece& key, const base::StringPiece& value);
  ~ScopedCrashKey();

 private:
  std::string key_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCrashKey);
};

typedef void (*SetCrashKeyValueFuncT)(const base::StringPiece&,
                                      const base::StringPiece&);
typedef void (*ClearCrashKeyValueFuncT)(const base::StringPiece&);

// Sets the function pointers that are used to integrate with the platform-
// specific crash reporting libraries.
BASE_EXPORT void SetCrashKeyReportingFunctions(
    SetCrashKeyValueFuncT set_key_func,
    ClearCrashKeyValueFuncT clear_key_func);

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_CRASH_LOGGING_H_
