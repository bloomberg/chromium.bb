// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_cert_fetcher.h"  // nogncheck

#include "base/strings/string_piece.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece input(reinterpret_cast<const char*>(data), size);
  SignedExchangeCertFetcher::GetCertChainFromMessage(input);
  return 0;
}

}  // namespace content
