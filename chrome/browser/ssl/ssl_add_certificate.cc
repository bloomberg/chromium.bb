// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_certificate.h"

#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "net/cert/x509_certificate.h"

namespace chrome {

void SSLAddCertificate(
    net::URLRequest* request,
    net::CertificateMimeType cert_type,
    const void* cert_data,
    size_t cert_size,
    int render_process_id,
    int render_view_id) {
  // Chromium only supports X.509 User certificates on non-Android
  // platforms. Note that this method should not be called for other
  // certificate mime types.
  if (cert_type != net::CERTIFICATE_MIME_TYPE_X509_USER_CERT)
    return;

  scoped_refptr<net::X509Certificate> cert;
  if (cert_data != NULL) {
    cert = net::X509Certificate::CreateFromBytes(
        reinterpret_cast<const char*>(cert_data), cert_size);
  }
  // NOTE: Passing a NULL cert pointer if |cert_data| was NULL is
  // intentional here.

  // The handler will run the UI and delete itself when it's finished.
  new SSLAddCertHandler(request, cert, render_process_id, render_view_id);
}

}  // namespace chrome
