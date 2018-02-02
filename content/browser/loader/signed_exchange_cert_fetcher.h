// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CERT_CETCHER_H_
#define CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CERT_CETCHER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SignedExchangeCertFetcher {
 public:
  // Parses a TLS 1.3 Certificate message containing X.509v3 certificates and
  // returns a vector of cert_data. Returns nullopt when failed to parse.
  static base::Optional<std::vector<base::StringPiece>> GetCertChainFromMessage(
      base::StringPiece message);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CERT_CETCHER_H_
