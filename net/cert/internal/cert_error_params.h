// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CERT_ERROR_PARAMS_H_
#define NET_CERT_INTERNAL_CERT_ERROR_PARAMS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {

namespace der {
class Input;
}

// CertErrorParams is a base class for describing extra parameters attached to
// a CertErrorNode.
//
// An example use for parameters is to identify the OID for an unconsumed
// critical extension. This parameter could then be pretty printed when
// diagnosing the error.
class NET_EXPORT CertErrorParams {
 public:
  CertErrorParams();
  virtual ~CertErrorParams();

  // Creates a representation of this parameter as a string, which may be
  // used for pretty printing the error.
  virtual std::string ToDebugString() const = 0;

  // TODO(crbug.com/634443): Add methods to access the underlying
  // structure, should they be needed for non-pretty-printing use cases.

 private:
  DISALLOW_COPY_AND_ASSIGN(CertErrorParams);
};

// TODO(crbug.com/634443): Should the underlying parameter classes be exposed
// so error consumers can access their data directly? (Without having to go
// through the generic virtuals).

// Creates a parameter object that holds a copy of |der1|, and names it |name1|
// in debug string outputs.
NET_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams1Der(
    const char* name1,
    const der::Input& der1);

// Same as CreateCertErrorParams1Der() but has a second DER blob.
NET_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams2Der(
    const char* name1,
    const der::Input& der1,
    const char* name2,
    const der::Input& der2);

// Creates a parameter object that holds a single size_t value. |name| is used
// when pretty-printing the parameters.
NET_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParamsSizeT(
    const char* name,
    size_t value);

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERT_ERROR_PARAMS_H_
