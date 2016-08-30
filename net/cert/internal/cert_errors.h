// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ----------------------------
// Overview of error design
// ----------------------------
//
// Certificate path validation may emit a sequence of errors/warnings. These
// are represented by |CertErrors|.
//
// |CertErrors| is basically just a sequence of errors. The order of the errors
// reflects when they were added.
//
// Each |CertError| has three parts:
//
//   * A unique identifier for the error/warning
//       - essentially an error code
//
//   * Optional parameters specific to this error type
//       - May identify relevant DER or OIDs in the certificate
//
//   * Optional context that describes where the error happened
//       - Which certificate or trust anchor were we processing when the error
//         was encountered?
//

#ifndef NET_CERT_INTERNAL_CERT_ERRORS_H_
#define NET_CERT_INTERNAL_CERT_ERRORS_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/der/input.h"

namespace base {
class Value;
}

namespace net {

class ParsedCertificate;
class TrustAnchor;

// Certificate error types are identified by null-terminated C-strings, with
// unique pointer values.
//
// Equality of CertErrorType is done using (pointer) equality and not string
// comparison.
//
// To ensure uniqueness define errors using the macro DEFINE_CERT_ERROR_TYPE().
using CertErrorType = const char*;

// TODO(crbug.com/634443): Implement this -- add magic to ensure that storage
//                         of identical strings isn't pool.
#define DEFINE_CERT_ERROR_TYPE(name, c_str_literal) \
  CertErrorType name = c_str_literal

// CertErrorParams is a base class for describing parameters for a particular
// CertErrorType.
//
// Parameters may be used to associate extra information with an error. An
// example use for parameters is to identify the OID for an unconsumed critical
// extension.
class NET_EXPORT CertErrorParams {
 public:
  CertErrorParams();
  virtual ~CertErrorParams();

  // Creates a representation of this parameter as a base::Value, which may be
  // used for pretty printing the error.
  virtual std::unique_ptr<base::Value> ToValue() const = 0;

  // TODO(crbug.com/634443): Add methods access the underlying structure.
  // ToValue() alone is not a great way to get at the data.

 private:
  DISALLOW_COPY_AND_ASSIGN(CertErrorParams);
};

// CertError represents a single error during path validation.
struct NET_EXPORT CertError {
  CertError();
  CertError(CertError&& other);
  ~CertError();

  // The "type" of the error. This describes the error class -- what is
  // typically done using an integer error code.
  CertErrorType type = nullptr;

  // This describes any parameter relevant to the error.
  std::unique_ptr<CertErrorParams> params;

  // TODO(crbug.com/634443): Add context (i.e. associated certificate/trust
  // anchor).
};

class NET_EXPORT CertErrors {
 public:
  CertErrors();
  ~CertErrors();

  void Add(CertErrorType type);

  void AddWithParam(CertErrorType type,
                    std::unique_ptr<CertErrorParams> params);

  void AddWith1DerParam(CertErrorType type, const der::Input& der1);
  void AddWith2DerParams(CertErrorType type,
                         const der::Input& der1,
                         const der::Input& der2);

  const std::vector<CertError>& errors() const { return errors_; }

 private:
  std::vector<CertError> errors_;

  DISALLOW_COPY_AND_ASSIGN(CertErrors);
};

// --------------------------
// Context scopers
// --------------------------

// TODO(crbug.com/634443): Implement.
class NET_EXPORT ScopedCertErrorsCertContext {
 public:
  ScopedCertErrorsCertContext(CertErrors* parent,
                              const ParsedCertificate* cert,
                              size_t i);
  ~ScopedCertErrorsCertContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCertErrorsCertContext);
};

// TODO(crbug.com/634443): Implement.
class NET_EXPORT ScopedCertErrorsTrustAnchorContext {
 public:
  ScopedCertErrorsTrustAnchorContext(CertErrors* parent,
                                     const TrustAnchor* trust_anchor);
  ~ScopedCertErrorsTrustAnchorContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCertErrorsTrustAnchorContext);
};

// --------------------------
// Error parameters
// --------------------------

class NET_EXPORT CertErrorParamsDer1 : public CertErrorParams {
 public:
  explicit CertErrorParamsDer1(const der::Input& der1);

  std::unique_ptr<base::Value> ToValue() const override;

 private:
  const std::string der1_;

  DISALLOW_COPY_AND_ASSIGN(CertErrorParamsDer1);
};

class NET_EXPORT CertErrorParamsDer2 : public CertErrorParams {
 public:
  CertErrorParamsDer2(const der::Input& der1, const der::Input& der2);

  std::unique_ptr<base::Value> ToValue() const override;

 private:
  const std::string der1_;
  const std::string der2_;

  DISALLOW_COPY_AND_ASSIGN(CertErrorParamsDer2);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERT_ERRORS_H_
