// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CERTIFICATE_PATTERN_MATCHER_H_
#define CHROMEOS_NETWORK_CERTIFICATE_PATTERN_MATCHER_H_

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"

namespace net {
class X509Certificate;
}

namespace chromeos {

class CertificatePattern;

namespace certificate_pattern {

// Fetches the matching certificate that has the latest valid start date.
// Returns a NULL refptr if there is no such match.
CHROMEOS_EXPORT scoped_refptr<net::X509Certificate> GetCertificateMatch(
    const CertificatePattern& pattern);

}  // namespace certificate_pattern

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CERTIFICATE_PATTERN_MATCHER_H_
