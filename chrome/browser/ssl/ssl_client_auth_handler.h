// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "content/browser/browser_thread.h"
#include "net/base/ssl_cert_request_info.h"

namespace net {
class URLRequest;
class X509Certificate;
}  // namespace net

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user.
// It is self-owned and deletes itself when the UI reports the user selection or
// when the net::URLRequest is cancelled.
class SSLClientAuthHandler
    : public base::RefCountedThreadSafe<SSLClientAuthHandler,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  SSLClientAuthHandler(net::URLRequest* request,
                       net::SSLCertRequestInfo* cert_request_info);

  // Asks the user to select a certificate and resumes the URL request with that
  // certificate.
  // Should only be called on the IO thread.
  void SelectCertificate();

  // Invoked when the request associated with this handler is cancelled.
  // Should only be called on the IO thread.
  void OnRequestCancelled();

  // Calls DoCertificateSelected on the I/O thread.
  // Called on the UI thread after the user has made a selection (which may
  // be long after DoSelectCertificate returns, if the UI is modeless/async.)
  void CertificateSelected(net::X509Certificate* cert);

  // Returns the SSLCertRequestInfo for this handler.
  net::SSLCertRequestInfo* cert_request_info() { return cert_request_info_; }

 private:
  friend class BrowserThread;
  friend class DeleteTask<SSLClientAuthHandler>;

  virtual ~SSLClientAuthHandler();

  // Notifies that the user has selected a cert.
  // Called on the IO thread.
  void DoCertificateSelected(net::X509Certificate* cert);

  // The net::URLRequest that triggered this client auth.
  net::URLRequest* request_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
