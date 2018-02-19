// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_header.h"

#include <utility>

#include "base/strings/string_piece.h"

namespace content {

SignedExchangeHeader::SignedExchangeHeader() = default;
SignedExchangeHeader::~SignedExchangeHeader() = default;

void SignedExchangeHeader::AddResponseHeader(base::StringPiece name,
                                             base::StringPiece value) {
  std::string name_string;
  std::string value_string;
  name.CopyToString(&name_string);
  value.CopyToString(&value_string);
  response_headers_[name_string] = value_string;
}

}  // namespace content
