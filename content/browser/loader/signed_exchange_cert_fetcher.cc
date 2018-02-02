// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_cert_fetcher.h"

#include "base/strings/string_piece.h"

namespace content {

namespace {

bool ConsumeByte(base::StringPiece* data, uint8_t* out) {
  if (data->empty())
    return false;
  *out = (*data)[0];
  data->remove_prefix(1);
  return true;
}

bool Consume2Bytes(base::StringPiece* data, uint16_t* out) {
  if (data->size() < 2)
    return false;
  *out = (static_cast<uint8_t>((*data)[0]) << 8) |
         static_cast<uint8_t>((*data)[1]);
  data->remove_prefix(2);
  return true;
}

bool Consume3Bytes(base::StringPiece* data, uint32_t* out) {
  if (data->size() < 3)
    return false;
  *out = (static_cast<uint8_t>((*data)[0]) << 16) |
         (static_cast<uint8_t>((*data)[1]) << 8) |
         static_cast<uint8_t>((*data)[2]);
  data->remove_prefix(3);
  return true;
}

}  // namespace

base::Optional<std::vector<base::StringPiece>>
SignedExchangeCertFetcher::GetCertChainFromMessage(base::StringPiece message) {
  uint8_t cert_request_context_size = 0;
  if (!ConsumeByte(&message, &cert_request_context_size)) {
    DVLOG(1) << "Can't read certificate request request context size.";
    return base::nullopt;
  }
  if (cert_request_context_size != 0) {
    DVLOG(1) << "Invalid certificate request context size: "
             << static_cast<int>(cert_request_context_size);
    return base::nullopt;
  }
  uint32_t cert_list_size = 0;
  if (!Consume3Bytes(&message, &cert_list_size)) {
    DVLOG(1) << "Can't read certificate list size.";
    return base::nullopt;
  }

  if (cert_list_size != message.length()) {
    DVLOG(1) << "Certificate list size error: cert_list_size=" << cert_list_size
             << " remaining=" << message.length();
    return base::nullopt;
  }

  std::vector<base::StringPiece> certs;
  while (!message.empty()) {
    uint32_t cert_data_size = 0;
    if (!Consume3Bytes(&message, &cert_data_size)) {
      DVLOG(1) << "Can't read certificate data size.";
      return base::nullopt;
    }
    if (message.length() < cert_data_size) {
      DVLOG(1) << "Certificate data size error: cert_data_size="
               << cert_data_size << " remaining=" << message.length();
      return base::nullopt;
    }
    certs.emplace_back(message.substr(0, cert_data_size));
    message.remove_prefix(cert_data_size);

    uint16_t extensions_size = 0;
    if (!Consume2Bytes(&message, &extensions_size)) {
      DVLOG(1) << "Can't read extensions size.";
      return base::nullopt;
    }
    if (message.length() < extensions_size) {
      DVLOG(1) << "Extensions size error: extensions_size=" << extensions_size
               << " remaining=" << message.length();
      return base::nullopt;
    }
    message.remove_prefix(extensions_size);
  }
  return certs;
}

}  // namespace content
