// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_
#define CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_

#include <windows.h>

#include <stdint.h>

#include <string>

#include "chrome_elf/third_party_dlls/logging_api.h"

namespace third_party_dlls {

// "static_cast<int>(LogStatus::value)" to access underlying value.
enum class LogStatus { kSuccess = 0, kCreateMutexFailure = 1, COUNT };

// Adds a module load attempt to the internal load log.
// - |log_type| indicates the type of logging.
// - |basename_hash| and |code_id_hash| must each point to a 20-byte buffer
//   holding a SHA-1 digest (of the module's basename and code identifier,
//   respectively).
// - For loads that are allowed, |full_image_path| indicates the full path of
//   the loaded image.
void LogLoadAttempt(LogType log_type,
                    const uint8_t* basename_hash,
                    const uint8_t* code_id_hash,
                    const char* full_image_path);

// Initialize internal logs.
LogStatus InitLogs();

// Removes initialization for use by tests.
void DeinitLogsForTesting();

}  // namespace third_party_dlls

#endif  // CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_
