// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"

// The type of certificate found for the module.
enum class CertificateType {
  // The module is not signed.
  NO_CERTIFICATE,
  // The module is signed and the certificate is in the module.
  CERTIFICATE_IN_FILE,
  // The module is signed and the certificate is in an external catalog.
  CERTIFICATE_IN_CATALOG,
};

// Information about the certificate of a file.
struct CertificateInfo {
  CertificateInfo();

  // The type of signature encountered.
  CertificateType type;

  // Path to the file containing the certificate. Empty if |type| is
  // NO_CERTIFICATE.
  base::FilePath path;

  // The "Subject" name of the certificate. This is the signer (ie,
  // "Google Inc." or "Microsoft Inc.").
  base::string16 subject;
};

// Extracts information about the certificate of the given |file|, populating
// |certificate_info|. It is expected that |certificate_info| be freshly
// constructed.
void GetCertificateInfo(const base::FilePath& file,
                        CertificateInfo* certificate_info);

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_INFO_UTIL_WIN_H_
