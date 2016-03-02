// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_piece.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

// Initialize ICU.
struct InitICU {
  InitICU() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

InitICU* init_icu = new InitICU();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Call GetDomainAndRegistry() twice - once with each filter type to ensure
  // both code paths are exercised.
  net::registry_controlled_domains::GetDomainAndRegistry(
      base::StringPiece(reinterpret_cast<const char*>(data), size),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  net::registry_controlled_domains::GetDomainAndRegistry(
      base::StringPiece(reinterpret_cast<const char*>(data), size),
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  return 0;
}
