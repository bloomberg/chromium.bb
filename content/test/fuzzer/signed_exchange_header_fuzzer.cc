// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "content/browser/web_package/signed_exchange_header.h"  // nogncheck

namespace content {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < SignedExchangeHeader::kEncodedHeaderLengthInBytes)
    return 0;
  auto encoded_length =
      base::make_span(data, SignedExchangeHeader::kEncodedHeaderLengthInBytes);
  size_t header_len = SignedExchangeHeader::ParseHeadersLength(encoded_length);
  data += SignedExchangeHeader::kEncodedHeaderLengthInBytes;
  size -= SignedExchangeHeader::kEncodedHeaderLengthInBytes;

  // Copy the header into a separate buffer so that out-of-bounds access can be
  // detected.
  std::vector<uint8_t> header(data, data + std::min(size, header_len));

  SignedExchangeHeader::Parse(
      base::make_span(header),
      base::RepeatingCallback<void(const std::string&)>());
  return 0;
}

}  // namespace content
