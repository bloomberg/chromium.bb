// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_certificate.h"

#include "net/android/network_library.h"

namespace chrome {

// Special case for Android here for several reasons:
//
// - The SSLAddCertHandler implementation currently only supports
//   CERTIFICATE_TYPE_X509_USER_CERT, but not other types, like
//   CERTIFICATE_TYPE_PKCS12_ARCHIVE which are required on this
//   platform.
//
// - Besides, SSLAddCertHandler tries to parse the certificate
//   by calling net::CertDatabase::CheckUserCert() which is not
//   implemented on Android, mainly because there is no API
//   provided by the system to do that properly.
//
// - The Android CertInstaller activity will check the certificate file
//   and display a toast (small fading dialog) to the user if it is
//   not valid, so the UI performed by SSLAddCertHandler would
//   be redundant.
void SSLAddCertificate(
    net::CertificateMimeType cert_type,
    const void* cert_data,
    size_t cert_size,
    int /* render_process_id */,
    int /* render_view_id */) {
  if (cert_size > 0) {
    // This launches a new activity which will run in a different process.
    // It handles all user interaction, so no need to do anything in the
    // browser UI thread here.
    net::android::StoreCertificate(cert_type, cert_data, cert_size);
  }
}

}  //  namespace chrome
