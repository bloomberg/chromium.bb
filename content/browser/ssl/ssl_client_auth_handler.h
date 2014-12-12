// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {
class ClientCertStore;
class URLRequest;
class X509Certificate;
}  // namespace net

namespace content {

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user. Should only be used on the IO thread. If the
// SSLClientAuthHandler is destroyed before the certificate is selected, the
// selection is canceled and the callback never called.
class SSLClientAuthHandler {
 public:
  using CertificateCallback = base::Callback<void(net::X509Certificate*)>;

  SSLClientAuthHandler(scoped_ptr<net::ClientCertStore> client_cert_store,
                       net::URLRequest* request,
                       net::SSLCertRequestInfo* cert_request_info,
                       const CertificateCallback& callback);
  ~SSLClientAuthHandler();

  // Selects a certificate and resumes the URL request with that certificate.
  void SelectCertificate();

 private:
  class Core;

  // Called when |core_| is done retrieving the cert list.
  void DidGetClientCerts();

  // Called when the user has selected a cert.
  void CertificateSelected(net::X509Certificate* cert);

  // A reference-counted core so the ClientCertStore may outlive
  // SSLClientAuthHandler if the handler is destroyed while an operation on the
  // ClientCertStore is in progress.
  scoped_refptr<Core> core_;

  // The net::URLRequest that triggered this client auth.
  net::URLRequest* request_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  // The callback to call when the certificate is selected.
  CertificateCallback callback_;

  base::WeakPtrFactory<SSLClientAuthHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
