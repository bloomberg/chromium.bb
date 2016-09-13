// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/cert_error_params.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/der/input.h"

namespace net {

namespace {

// Parameters subclass for describing (and pretty-printing) 1 or 2 DER
// blobs. It makes a copy of the der::Inputs.
class CertErrorParams2Der : public CertErrorParams {
 public:
  CertErrorParams2Der(const char* name1,
                      const der::Input& der1,
                      const char* name2,
                      const der::Input& der2)
      : name1_(name1),
        der1_(der1.AsString()),
        name2_(name2),
        der2_(der2.AsString()) {}

  std::string ToDebugString() const override {
    std::string result;
    AppendDer(name1_, der1_, &result);
    if (name2_) {
      result += "\n";
      AppendDer(name2_, der2_, &result);
    }
    return result;
  }

 private:
  static void AppendDer(const char* name,
                        const std::string& der,
                        std::string* out) {
    *out += name;
    *out += ": " + base::HexEncode(der.data(), der.size());
  }

  const char* name1_;
  std::string der1_;

  const char* name2_;
  std::string der2_;
};

// Parameters subclass for describing (and pretty-printing) a single size_t.
class CertErrorParamsSizeT : public CertErrorParams {
 public:
  CertErrorParamsSizeT(const char* name, size_t value)
      : name_(name), value_(value) {}

  std::string ToDebugString() const override {
    return name_ + std::string(": ") + base::SizeTToString(value_);
  }

 private:
  const char* name_;
  size_t value_;
};

}  // namespace

CertErrorParams::CertErrorParams() = default;
CertErrorParams::~CertErrorParams() = default;

std::unique_ptr<CertErrorParams> CreateCertErrorParams1Der(
    const char* name1,
    const der::Input& der1) {
  return base::MakeUnique<CertErrorParams2Der>(name1, der1, nullptr,
                                               der::Input());
}

std::unique_ptr<CertErrorParams> CreateCertErrorParams2Der(
    const char* name1,
    const der::Input& der1,
    const char* name2,
    const der::Input& der2) {
  return base::MakeUnique<CertErrorParams2Der>(name1, der1, name2, der2);
}

std::unique_ptr<CertErrorParams> CreateCertErrorParamsSizeT(const char* name,
                                                            size_t value) {
  return base::MakeUnique<CertErrorParamsSizeT>(name, value);
}

}  // namespace net
