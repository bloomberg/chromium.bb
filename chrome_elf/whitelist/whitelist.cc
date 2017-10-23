// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/whitelist/whitelist.h"

#include <assert.h>

#include "chrome_elf/nt_registry/nt_registry.h"
#include "chrome_elf/whitelist/whitelist_ime.h"

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

  // TODO(pennymac): As sources are added, consider multi-threaded init.
  // Source: Input Method Editors (IMEs)
  IMEStatus rc = InitIMEs();
  if (rc != IMEStatus::kSuccess)
    return false;

  // Record initialization.
  g_whitelist_initialized = true;

  return true;
}

}  // namespace whitelist
