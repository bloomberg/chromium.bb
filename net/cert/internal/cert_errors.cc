// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/cert_errors.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/der/input.h"

namespace net {

namespace {

std::string HexEncodeString(const std::string& bytes) {
  return base::HexEncode(bytes.data(), bytes.size());
}

}  // namespace

CertErrorParams::CertErrorParams() = default;
CertErrorParams::~CertErrorParams() = default;

CertError::CertError() = default;
CertError::~CertError() = default;
CertError::CertError(CertError&& other) = default;

CertErrors::CertErrors() = default;

CertErrors::~CertErrors() = default;

void CertErrors::Add(CertErrorType type) {
  AddWithParam(type, nullptr);
}

void CertErrors::AddWithParam(CertErrorType type,
                              std::unique_ptr<CertErrorParams> params) {
  errors_.resize(errors_.size() + 1);
  errors_.back().type = type;
  errors_.back().params = std::move(params);
}

void CertErrors::AddWith1DerParam(CertErrorType type, const der::Input& der1) {
  AddWithParam(type, base::MakeUnique<CertErrorParamsDer1>(der1));
}

void CertErrors::AddWith2DerParams(CertErrorType type,
                                   const der::Input& der1,
                                   const der::Input& der2) {
  AddWithParam(type, base::MakeUnique<CertErrorParamsDer2>(der1, der2));
}

ScopedCertErrorsCertContext::ScopedCertErrorsCertContext(
    CertErrors* parent,
    const ParsedCertificate* cert,
    size_t i) {
  // TODO(crbug.com/634443): Implement.
}

ScopedCertErrorsCertContext::~ScopedCertErrorsCertContext() {
  // TODO(crbug.com/634443): Implement.
}

ScopedCertErrorsTrustAnchorContext::ScopedCertErrorsTrustAnchorContext(
    CertErrors* parent,
    const TrustAnchor* trust_anchor) {
  // TODO(crbug.com/634443): Implement.
}

ScopedCertErrorsTrustAnchorContext::~ScopedCertErrorsTrustAnchorContext() {
  // TODO(crbug.com/634443): Implement.
}

CertErrorParamsDer1::CertErrorParamsDer1(const der::Input& der1)
    : der1_(der1.AsString()) {}

std::unique_ptr<base::Value> CertErrorParamsDer1::ToValue() const {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString("der1", HexEncodeString(der1_));
  return std::move(dict);
}

CertErrorParamsDer2::CertErrorParamsDer2(const der::Input& der1,
                                         const der::Input& der2)
    : der1_(der1.AsString()), der2_(der2.AsString()) {}

std::unique_ptr<base::Value> CertErrorParamsDer2::ToValue() const {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString("der1", HexEncodeString(der1_));
  dict->SetString("der2", HexEncodeString(der2_));
  return std::move(dict);
}

}  // namespace net
