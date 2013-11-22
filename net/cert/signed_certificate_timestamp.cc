// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

bool SignedCertificateTimestamp::LessThan::operator()(
    const scoped_refptr<SignedCertificateTimestamp>& lhs,
    const scoped_refptr<SignedCertificateTimestamp>& rhs) const {
  if (lhs.get() == rhs.get())
    return false;
  if (lhs->signature.signature_data != rhs->signature.signature_data)
    return lhs->signature.signature_data < rhs->signature.signature_data;
  if (lhs->log_id != rhs->log_id)
    return lhs->log_id < rhs->log_id;
  if (lhs->timestamp != rhs->timestamp)
    return lhs->timestamp < rhs->timestamp;
  if (lhs->extensions != rhs->extensions)
    return lhs->extensions < rhs->extensions;
  return lhs->version < rhs->version;
}

SignedCertificateTimestamp::SignedCertificateTimestamp() {}

SignedCertificateTimestamp::~SignedCertificateTimestamp() {}

LogEntry::LogEntry() {}

LogEntry::~LogEntry() {}

void LogEntry::Reset() {
  type = LogEntry::LOG_ENTRY_TYPE_X509;
  leaf_certificate.clear();
  tbs_certificate.clear();
}

DigitallySigned::DigitallySigned() {}

DigitallySigned::~DigitallySigned() {}

}  // namespace ct

}  // namespace net
