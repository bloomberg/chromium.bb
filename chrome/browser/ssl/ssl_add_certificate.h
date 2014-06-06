// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ADD_CERTIFICATE_H_
#define CHROME_BROWSER_SSL_SSL_ADD_CERTIFICATE_H_

#include "base/basictypes.h"
#include "net/base/mime_util.h"

namespace net {
class URLRequest;
}  // namespace net

namespace chrome {

// This method is used to add a new certificate file.
//
void SSLAddCertificate(
    net::CertificateMimeType cert_type,
    const void* cert_data,
    size_t cert_size,
    int render_process_id,
    int render_view_id);

}  // namespace chrome

#endif  // CHROME_BROWSER_SSL_SSL_ADD_CERTIFICATE_H_
