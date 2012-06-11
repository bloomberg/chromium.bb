// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client Auth is not implemented on Android yet.
#include "chrome/browser/ssl_client_certificate_selector.h"

#include "base/logging.h"

///////////////////////////////////////////////////////////////////////////////
// SSLClientCertificateSelector public interface:

namespace browser {

void ShowSSLClientCertificateSelector(
    TabContents* tab_contents,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
  NOTIMPLEMENTED();
}

}  // namespace browser
