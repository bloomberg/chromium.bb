// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include <memory>

#include "base/debug/crash_logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/client_info.h"

namespace child_process_logging {

namespace {

// exported in breakpad_win.cc/crashpad_win.cc:
//    void __declspec(dllexport) __cdecl SetCrashKeyValueImpl.
typedef void(__cdecl* SetCrashKeyValue)(const wchar_t*, const wchar_t*);

// exported in breakpad_win.cc/crashpad_win.cc:
//    void __declspec(dllexport) __cdecl ClearCrashKeyValueImpl.
typedef void(__cdecl* ClearCrashKeyValue)(const wchar_t*);

void SetCrashKeyValueTrampoline(const base::StringPiece& key,
                                const base::StringPiece& value) {
  static SetCrashKeyValue set_crash_key = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<SetCrashKeyValue>(
        elf_module ? GetProcAddress(elf_module, "SetCrashKeyValueImpl")
                   : nullptr);
  }();
  if (set_crash_key) {
    (set_crash_key)(base::UTF8ToWide(key).data(),
                    base::UTF8ToWide(value).data());
  }
}

void ClearCrashKeyValueTrampoline(const base::StringPiece& key) {
  static ClearCrashKeyValue clear_crash_key = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<ClearCrashKeyValue>(
        elf_module ? GetProcAddress(elf_module, "ClearCrashKeyValueImpl")
                   : nullptr);
  }();
  if (clear_crash_key)
    (clear_crash_key)(base::UTF8ToWide(key).data());
}

}  // namespace

void Init() {
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValueTrampoline,
                                             &ClearCrashKeyValueTrampoline);

  // This would be handled by BreakpadClient::SetCrashClientIdFromGUID(), but
  // because of the aforementioned issue, crash keys aren't ready yet at the
  // time of Breakpad initialization, load the client id backed up in Google
  // Update settings instead.
  // Please note if we are using Crashpad via chrome_elf then we need to call
  // into chrome_elf to pass in the client id.
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();

  // Set the client id in chrome_elf if it is loaded. We should not be
  // registering crash keys in this case as that would already have been
  // done by chrome_elf.
  HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
  if (elf_module) {
// TODO(ananta)
// Remove this when the change to not require crash key registration lands.
// Please note that we are registering the crash keys twice if chrome_elf is
// loaded. Once in chrome_elf and once in the current module. Alternatively
// we could implement a crash key lookup trampoline which defers to
// chrome_elf. We decided to go with the duplicate key registration for
// simplicity.
#if !defined(COMPONENT_BUILD)
    crash_keys::RegisterChromeCrashKeys();
#endif
    using SetMetricsClientIdFunction = void (*)(const char* client_id);
    SetMetricsClientIdFunction set_metrics_id_fn =
        reinterpret_cast<SetMetricsClientIdFunction>(
            ::GetProcAddress(elf_module, "SetMetricsClientId"));
    DCHECK(set_metrics_id_fn);
    set_metrics_id_fn(client_info ? client_info->client_id.c_str() : nullptr);
  } else {
    // TODO(ananta)
    // Remove this when the change to not require crash key registration lands.
    crash_keys::RegisterChromeCrashKeys();
    if (client_info)
      crash_keys::SetMetricsClientIdFromGUID(client_info->client_id);
  }
}

}  // namespace child_process_logging
