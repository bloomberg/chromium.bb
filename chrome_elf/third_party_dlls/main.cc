// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/third_party_dlls/main.h"

#include <assert.h>

#include "chrome_elf/nt_registry/nt_registry.h"
#include "chrome_elf/third_party_dlls/imes.h"
#include "chrome_elf/third_party_dlls/logs.h"
#include "chrome_elf/third_party_dlls/packed_list_file.h"

namespace {

// Record if all the third-party DLL management code was successfully
// initialized, so processes can easily determine if it is enabled for them.
bool g_third_party_initialized = false;

}  // namespace

namespace third_party_dlls {

bool IsThirdPartyInitialized() {
  return g_third_party_initialized;
}

bool Init() {
  // Debug check: Init should not be called more than once.
  assert(!g_third_party_initialized);

  // TODO(pennymac): As work is added, consider multi-threaded init.
  // TODO(pennymac): Handle return status codes for UMA.
  if (InitIMEs() != IMEStatus::kSuccess ||
      InitFromFile() != FileStatus::kSuccess ||
      InitLogs() != LogStatus::kSuccess) {
    return false;
  }

  // Record initialization.
  g_third_party_initialized = true;

  return true;
}

}  // namespace third_party_dlls
