// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_parser.h"

namespace content {

SignedExchangeParser::SignedExchangeParser(HeaderCallback header_callback)
    : header_callback_(std::move(header_callback)) {}

SignedExchangeParser::~SignedExchangeParser() = default;

SignedExchangeParser::ConsumeResult SignedExchangeParser::TryConsume(
    base::span<const uint8_t> input,
    size_t* consumed_bytes,
    base::span<uint8_t> output,
    size_t* written) {
  NOTIMPLEMENTED();
  return ConsumeResult::SUCCESS;
}

}  // namespace content
