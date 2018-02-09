// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_header.h"

#include <utility>

namespace content {

SignedExchangeHeader::SignedExchangeHeader() = default;
SignedExchangeHeader::~SignedExchangeHeader() = default;

void SignedExchangeHeader::AddResponseHeader(base::StringPiece name,
                                             base::StringPiece value) {
  response_headers_.insert(std::make_pair(name, value));
}

}  // namespace content
