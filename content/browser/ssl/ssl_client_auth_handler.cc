// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_client_auth_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_request_info.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

class ClientCertificateDelegateImpl : public ClientCertificateDelegate {
 public:
  explicit ClientCertificateDelegateImpl(
      const base::WeakPtr<SSLClientAuthHandler>& handler)
      : handler_(handler), continue_called_(false) {}

  ~ClientCertificateDelegateImpl() override {
    if (!continue_called_) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&SSLClientAuthHandler::CancelCertificateSelection,
                     handler_));
    }
  }

  // ClientCertificateDelegate implementation:
  void ContinueWithCertificate(net::X509Certificate* cert) override {
    DCHECK(!continue_called_);
    continue_called_ = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SSLClientAuthHandler::ContinueWithCertificate, handler_,
                   base::RetainedRef(cert)));
  }

 private:
  base::WeakPtr<SSLClientAuthHandler> handler_;
  bool continue_called_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertificateDelegateImpl);
};

void SelectCertificateOnUIThread(
    const ResourceRequestInfo::WebContentsGetter& wc_getter,
    net::SSLCertRequestInfo* cert_request_info,
    net::CertificateList client_certs,
    const base::WeakPtr<SSLClientAuthHandler>& handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<ClientCertificateDelegate> delegate(
      new ClientCertificateDelegateImpl(handler));

  WebContents* web_contents = wc_getter.Run();
  if (!web_contents)
    return;

  GetContentClient()->browser()->SelectClientCertificate(
      web_contents, cert_request_info, std::move(client_certs),
      std::move(delegate));
}

}  // namespace

// A reference-counted core to allow the ClientCertStore and SSLCertRequestInfo
// to outlive SSLClientAuthHandler if needbe.
class SSLClientAuthHandler::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(const base::WeakPtr<SSLClientAuthHandler>& handler,
       std::unique_ptr<net::ClientCertStore> client_cert_store,
       net::SSLCertRequestInfo* cert_request_info)
      : handler_(handler),
        client_cert_store_(std::move(client_cert_store)),
        cert_request_info_(cert_request_info) {}

  bool has_client_cert_store() const { return !!client_cert_store_; }

  void GetClientCerts() {
    if (client_cert_store_) {
      // TODO(davidben): This is still a cyclical ownership where
      // GetClientCerts' requirement that |client_cert_store_| remains alive
      // until the call completes is maintained by the reference held in the
      // callback.
      client_cert_store_->GetClientCerts(
          *cert_request_info_,
          base::Bind(&SSLClientAuthHandler::Core::DidGetClientCerts, this));
    } else {
      DidGetClientCerts(net::CertificateList());
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;

  ~Core() {}

  // Called when |client_cert_store_| is done retrieving the cert list.
  void DidGetClientCerts(net::CertificateList client_certs) {
    if (handler_)
      handler_->DidGetClientCerts(std::move(client_certs));
  }

  base::WeakPtr<SSLClientAuthHandler> handler_;
  std::unique_ptr<net::ClientCertStore> client_cert_store_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
};

SSLClientAuthHandler::SSLClientAuthHandler(
    std::unique_ptr<net::ClientCertStore> client_cert_store,
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler::Delegate* delegate)
    : request_(request),
      cert_request_info_(cert_request_info),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  core_ = new Core(weak_factory_.GetWeakPtr(), std::move(client_cert_store),
                   cert_request_info_.get());
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
}

void SSLClientAuthHandler::SelectCertificate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |core_| will call DidGetClientCerts when done.
  core_->GetClientCerts();
}

// static
void SSLClientAuthHandler::ContinueWithCertificate(
    const base::WeakPtr<SSLClientAuthHandler>& handler,
    net::X509Certificate* cert) {
  if (handler)
    handler->delegate_->ContinueWithCertificate(cert);
}

// static
void SSLClientAuthHandler::CancelCertificateSelection(
    const base::WeakPtr<SSLClientAuthHandler>& handler) {
  if (handler)
    handler->delegate_->CancelCertificateSelection();
}

void SSLClientAuthHandler::DidGetClientCerts(
    net::CertificateList client_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note that if |client_cert_store_| is NULL, we intentionally fall through to
  // SelectCertificateOnUIThread. This is for platforms where the client cert
  // matching is not performed by Chrome. Those platforms handle the cert
  // matching before showing the dialog.
  if (core_->has_client_cert_store() && client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    //
    // TODO(davidben): The WebContents-less check on the UI thread should come
    // before checking ClientCertStore; ClientCertStore itself should probably
    // be handled by the embedder (https://crbug.com/394131), especially since
    // this doesn't work on Android (https://crbug.com/345641).
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SSLClientAuthHandler::ContinueWithCertificate,
                   weak_factory_.GetWeakPtr(), nullptr));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SelectCertificateOnUIThread,
                 ResourceRequestInfo::ForRequest(request_)
                     ->GetWebContentsGetterForRequest(),
                 base::RetainedRef(cert_request_info_), std::move(client_certs),
                 weak_factory_.GetWeakPtr()));
}

}  // namespace content
