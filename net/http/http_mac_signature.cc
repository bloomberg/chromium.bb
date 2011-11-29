// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_mac_signature.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "crypto/hmac.h"

namespace net {

namespace {

const char kSHA1Name[] = "hmac-sha-1";
const char kSHA256Name[] = "hmac-sha-256";
const int kNonceLength = 64/8;

bool IsPlainStringCharacter(char character) {
  return character == 0x20 || character == 0x21 ||
    (character >= 0x23 && character <= 0x5B) ||
    (character >= 0x5D && character <= 0x7E);
}

bool IsPlainString(const std::string& string) {
  for (size_t i = 0; i < string.size(); ++i) {
    if (!IsPlainStringCharacter(string[i]))
      return false;
  }
  return true;
}

std::string GenerateNonce() {
  std::string nonce;
  bool result = base::Base64Encode(
      base::RandBytesAsString(kNonceLength), &nonce);
  DCHECK(result);
  return nonce;
}

}

HttpMacSignature::HttpMacSignature()
    : mac_algorithm_(crypto::HMAC::SHA1) {
}

HttpMacSignature::~HttpMacSignature() {
}

bool HttpMacSignature::AddStateInfo(const std::string& id,
                                    const base::Time& creation_date,
                                    const std::string& mac_key,
                                    const std::string& mac_algorithm) {
  DCHECK(id_.empty());

  if (!IsPlainString(id) || id.empty() ||
      creation_date.is_null() ||
      mac_key.empty() ||
      mac_algorithm.empty()) {
    return false;
  }

  if (mac_algorithm == kSHA1Name)
    mac_algorithm_ = crypto::HMAC::SHA1;
  else if (mac_algorithm == kSHA256Name)
    mac_algorithm_ = crypto::HMAC::SHA256;
  else
    return false;

  id_ = id;
  creation_date_ = creation_date;
  mac_key_ = mac_key;
  return true;
}

bool HttpMacSignature::AddHttpInfo(const std::string& method,
                                   const std::string& request_uri,
                                   const std::string& host,
                                   int port) {
  DCHECK(method_.empty());

  if (!IsPlainString(method) || method.empty() ||
      !IsPlainString(request_uri) || request_uri.empty() ||
      !IsPlainString(host) || host.empty() ||
      port <= 0 || port > 65535) {
    return false;
  }

  method_ = StringToUpperASCII(method);
  request_uri_ = request_uri;
  host_ = StringToLowerASCII(host);
  port_ = base::IntToString(port);
  return true;
}

bool HttpMacSignature::GenerateAuthorizationHeader(std::string* header) {
  if (id_.empty()) {
    NOTREACHED() << "Call AddStateInfo first.";
    return false;
  }
  if (method_.empty()) {
    NOTREACHED() << "Call AddHttpInfo first.";
    return false;
  }

  std::string age = base::Int64ToString(
      (base::Time::Now() - creation_date_).InSeconds());
  std::string nonce = GenerateNonce();

  return GenerateHeaderString(age, nonce, header);
}

bool HttpMacSignature::GenerateHeaderString(const std::string& age,
                                            const std::string& nonce,
                                            std::string* header) {
  std::string mac;
  if (!GenerateMAC(age, nonce, &mac))
    return false;

  DCHECK(IsPlainString(age));
  DCHECK(IsPlainString(nonce));
  DCHECK(IsPlainString(mac));

  *header = "MAC id=\"" + id_ +
      "\", nonce=\"" + age + ":" + nonce +
      "\", mac=\"" + mac + "\"";
  return true;
}

std::string HttpMacSignature::GenerateNormalizedRequest(
    const std::string& age,
    const std::string& nonce) {
  const std::string kNewLine = "\n";

  std::string normalized_request = age + ":" + nonce + kNewLine;
  normalized_request += method_ + kNewLine;
  normalized_request += request_uri_ + kNewLine;
  normalized_request += host_ + kNewLine;
  normalized_request += port_ + kNewLine;
  normalized_request += kNewLine;
  normalized_request += kNewLine;

  return normalized_request;
}

bool HttpMacSignature::GenerateMAC(const std::string& age,
                                   const std::string& nonce,
                                   std::string* mac) {
  std::string request = GenerateNormalizedRequest(age, nonce);

  crypto::HMAC hmac(mac_algorithm_);
  if (!hmac.Init(mac_key_)) {
    NOTREACHED();
    return false;
  }

  std::string signature;
  size_t length = hmac.DigestLength();
  DCHECK_GT(length, 0u);
  if (!hmac.Sign(request,
          // We need the + 1 here not because the call will write a trailing \0,
          // but so that signature.length() is correctly set to |length|.
          reinterpret_cast<unsigned char*>(WriteInto(&signature, length + 1)),
          length)) {
    NOTREACHED();
    return false;
  }

  std::string encoded_signature;
  if (!base::Base64Encode(signature, &encoded_signature)) {
    NOTREACHED();
    return false;
  }

  mac->swap(encoded_signature);
  return true;
}

}  // namespace net
