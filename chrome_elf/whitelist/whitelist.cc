// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/whitelist/whitelist.h"

#include <assert.h>

#include "chrome_elf/nt_registry/nt_registry.h"
#include "chrome_elf/whitelist/whitelist_file.h"
#include "chrome_elf/whitelist/whitelist_ime.h"
#include "chrome_elf/whitelist/whitelist_log.h"

namespace {

// Record if the whitelist was successfully initialized so processes can easily
// determine if the whitelist is enabled for them.
bool g_whitelist_initialized = false;

}  // namespace

namespace whitelist {

bool IsWhitelistInitialized() {
  return g_whitelist_initialized;
}

bool Init() {
  // Debug check: Init should not be called more than once.
  assert(!g_whitelist_initialized);

  // TODO(pennymac): As work is added, consider multi-threaded init.
  // TODO(pennymac): Handle return status codes for UMA.
  if (InitIMEs() != IMEStatus::kSuccess ||
      InitFromFile() != FileStatus::kSuccess ||
      InitLogs() != LogStatus::kSuccess) {
    return false;
  }

  // Record initialization.
  g_whitelist_initialized = true;

  return true;
}

}  // namespace whitelist
