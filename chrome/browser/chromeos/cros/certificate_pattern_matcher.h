// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_MATCHER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_MATCHER_H_

#include "base/memory/ref_counted.h"

namespace net {
class X509Certificate;
}

namespace chromeos {

class CertificatePattern;

// Fetches the matching certificate that has the latest valid start date.
// Returns a NULL refptr if there is no such match.
scoped_refptr<net::X509Certificate> GetCertificateMatch(
    const CertificatePattern& pattern);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_MATCHER_H_
