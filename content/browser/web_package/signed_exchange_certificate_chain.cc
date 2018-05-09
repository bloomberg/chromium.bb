// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_certificate_chain.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/public/common/content_switches.h"
#include "net/cert/x509_certificate.h"

namespace content {

namespace {

bool ConsumeByte(base::span<const uint8_t>* data, uint8_t* out) {
  if (data->empty())
    return false;
  *out = (*data)[0];
  *data = data->subspan(1);
  return true;
}

bool Consume2Bytes(base::span<const uint8_t>* data, uint16_t* out) {
  if (data->size() < 2)
    return false;
  *out = ((*data)[0] << 8) | (*data)[1];
  *data = data->subspan(2);
  return true;
}

bool Consume3Bytes(base::span<const uint8_t>* data, uint32_t* out) {
  if (data->size() < 3)
    return false;
  *out = ((*data)[0] << 16) | ((*data)[1] << 8) | (*data)[2];
  *data = data->subspan(3);
  return true;
}

}  // namespace

// static
std::unique_ptr<SignedExchangeCertificateChain>
SignedExchangeCertificateChain::Parse(
    base::span<const uint8_t> cert_response_body) {
  base::Optional<std::vector<base::StringPiece>> der_certs =
      GetCertChainFromMessage(cert_response_body);
  if (!der_certs)
    return nullptr;

  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(*der_certs);
  if (!cert)
    return nullptr;
  return base::WrapUnique(new SignedExchangeCertificateChain(cert));
}

// static
base::Optional<std::vector<base::StringPiece>>
SignedExchangeCertificateChain::GetCertChainFromMessage(
    base::span<const uint8_t> message) {
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

  if (cert_list_size != message.size()) {
    DVLOG(1) << "Certificate list size error: cert_list_size=" << cert_list_size
             << " remaining=" << message.size();
    return base::nullopt;
  }

  std::vector<base::StringPiece> certs;
  while (!message.empty()) {
    uint32_t cert_data_size = 0;
    if (!Consume3Bytes(&message, &cert_data_size)) {
      DVLOG(1) << "Can't read certificate data size.";
      return base::nullopt;
    }
    if (message.size() < cert_data_size) {
      DVLOG(1) << "Certificate data size error: cert_data_size="
               << cert_data_size << " remaining=" << message.size();
      return base::nullopt;
    }
    certs.emplace_back(base::StringPiece(
        reinterpret_cast<const char*>(message.data()), cert_data_size));
    message = message.subspan(cert_data_size);

    uint16_t extensions_size = 0;
    if (!Consume2Bytes(&message, &extensions_size)) {
      DVLOG(1) << "Can't read extensions size.";
      return base::nullopt;
    }
    if (message.size() < extensions_size) {
      DVLOG(1) << "Extensions size error: extensions_size=" << extensions_size
               << " remaining=" << message.size();
      return base::nullopt;
    }
    message = message.subspan(extensions_size);
  }
  return certs;
}

SignedExchangeCertificateChain::SignedExchangeCertificateChain(
    scoped_refptr<net::X509Certificate> cert)
    : cert_(cert) {
  DCHECK(cert);
}

SignedExchangeCertificateChain::~SignedExchangeCertificateChain() = default;

}  // namespace content
