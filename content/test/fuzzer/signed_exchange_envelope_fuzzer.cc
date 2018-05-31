// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "content/browser/web_package/signed_exchange_envelope.h"  // nogncheck
#include "content/browser/web_package/signed_exchange_prologue.h"  // nogncheck

namespace content {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < SignedExchangePrologue::kEncodedLengthInBytes)
    return 0;
  auto encoded_length =
      base::make_span(data, SignedExchangePrologue::kEncodedLengthInBytes);
  size_t header_len =
      SignedExchangePrologue::ParseEncodedLength(encoded_length);
  data += SignedExchangePrologue::kEncodedLengthInBytes;
  size -= SignedExchangePrologue::kEncodedLengthInBytes;

  // Copy the header into a separate buffer so that out-of-bounds access can be
  // detected.
  std::vector<uint8_t> header(data, data + std::min(size, header_len));

  SignedExchangeEnvelope::Parse(base::make_span(header),
                                nullptr /* devtools_proxy */);
  return 0;
}

}  // namespace content
