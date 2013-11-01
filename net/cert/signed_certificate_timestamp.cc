// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

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
