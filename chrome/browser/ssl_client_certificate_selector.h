// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#pragma once

class SSLClientAuthHandler;
class TabContents;

namespace net {
class SSLCertRequestInfo;
}

namespace browser {

// Opens a constrained SSL client certificate selection dialog under |parent|,
// offering certificates from |cert_request_info|. When the user has made a
// selection, the dialog will report back to |delegate|. |delegate| is notified
// when the dialog closes in call cases; if the user cancels the dialog, we call
// with a NULL certificate.
//
// Note: constrained dialog currently only implemented on Linux and OS X. On
// Windows, a window-modal dialog will be used.
void ShowSSLClientCertificateSelector(
    TabContents* parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate);

}  // namespace browser

#endif  // CHROME_BROWSER_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
