// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/ssl/server_bound_cert_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace chromeos {

namespace {

class ProfileAuthDataTransferer {
 public:
  ProfileAuthDataTransferer(
      content::BrowserContext* from_context,
      content::BrowserContext* to_context,
      bool transfer_auth_cookies_and_server_bound_certs,
      const base::Closure& completion_callback);

  void BeginTransfer();

 private:
  void BeginTransferOnIOThread();

  // Transfer the proxy auth cache from |from_context_| to |to_context_|. If
  // the user was required to authenticate with a proxy during login, this
  // authentication information will be transferred into the user's session.
  void TransferProxyAuthCache();

  // Retrieve the contents of |from_context_|'s cookie jar. When the retrieval
  // finishes, OnCookiesToTransferRetrieved will be called with the result.
  void RetrieveCookiesToTransfer();

  // Callback that receives the contents of |from_context_|'s cookie jar. Calls
  // MaybeTransferCookiesAndCerts() to try and perform the transfer.
  void OnCookiesToTransferRetrieved(const net::CookieList& cookies_to_transfer);

  // Retrieve |from_context_|'s server bound certs. When the retrieval finishes,
  // OnServerBoundCertsToTransferRetrieved will be called with the result.
  void RetrieveServerBoundCertsToTransfer();

  // Callback that receives |from_context_|'s server bound certs. Calls
  // MaybeTransferCookiesAndCerts() to try and perform the transfer.
  void OnServerBoundCertsToTransferRetrieved(
      const net::ServerBoundCertStore::ServerBoundCertList& certs_to_transfer);

  // If both auth cookies and server bound certs have been retrieved from
  // |from_context| already, retrieve the contents of |to_context|'s cookie jar
  // as well, allowing OnTargetCookieJarContentsRetrieved() to perform the
  // actual transfer.
  void MaybeTransferCookiesAndCerts();

  // Transfer auth cookies and server bound certificates to the user's
  // |to_context_| if the user's cookie jar is empty. Call Finish() when done.
  void OnTargetCookieJarContentsRetrieved(
      const net::CookieList& target_cookies);

  // Post the |completion_callback_| to the UI thread and delete |this|.
  void Finish();

  scoped_refptr<net::URLRequestContextGetter> from_context_;
  scoped_refptr<net::URLRequestContextGetter> to_context_;
  bool transfer_auth_cookies_and_server_bound_certs_;
  base::Closure completion_callback_;

  net::CookieList cookies_to_transfer_;
  net::ServerBoundCertStore::ServerBoundCertList certs_to_transfer_;

  bool got_cookies_;
  bool got_server_bound_certs_;
};

ProfileAuthDataTransferer::ProfileAuthDataTransferer(
    content::BrowserContext* from_context,
    content::BrowserContext* to_context,
    bool transfer_auth_cookies_and_server_bound_certs,
    const base::Closure& completion_callback)
    : from_context_(from_context->GetRequestContext()),
      to_context_(to_context->GetRequestContext()),
      transfer_auth_cookies_and_server_bound_certs_(
          transfer_auth_cookies_and_server_bound_certs),
      completion_callback_(completion_callback),
      got_cookies_(false),
      got_server_bound_certs_(false) {
}

void ProfileAuthDataTransferer::BeginTransfer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we aren't transferring auth cookies or server bound certificates, post
  // the completion callback immediately. Otherwise, it will be called when both
  // auth cookies and server bound certificates have been transferred.
  if (!transfer_auth_cookies_and_server_bound_certs_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion_callback_);
    // Null the callback so that when Finish is called, the callback won't be
    // called again.
    completion_callback_.Reset();
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ProfileAuthDataTransferer::BeginTransferOnIOThread,
                 base::Unretained(this)));
}

void ProfileAuthDataTransferer::BeginTransferOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TransferProxyAuthCache();
  if (transfer_auth_cookies_and_server_bound_certs_) {
    RetrieveCookiesToTransfer();
    RetrieveServerBoundCertsToTransfer();
  } else {
    Finish();
  }
}

void ProfileAuthDataTransferer::TransferProxyAuthCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HttpAuthCache* new_cache = to_context_->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  new_cache->UpdateAllFrom(*from_context_->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache());
}

void ProfileAuthDataTransferer::RetrieveCookiesToTransfer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* from_store =
      from_context_->GetURLRequestContext()->cookie_store();
  net::CookieMonster* from_monster = from_store->GetCookieMonster();
  from_monster->SetKeepExpiredCookies();
  from_monster->GetAllCookiesAsync(
      base::Bind(&ProfileAuthDataTransferer::OnCookiesToTransferRetrieved,
                 base::Unretained(this)));
}

void ProfileAuthDataTransferer::OnCookiesToTransferRetrieved(
    const net::CookieList& cookies_to_transfer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  got_cookies_ = true;
  cookies_to_transfer_ = cookies_to_transfer;
  MaybeTransferCookiesAndCerts();
}

void ProfileAuthDataTransferer::RetrieveServerBoundCertsToTransfer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::ServerBoundCertService* from_service =
      from_context_->GetURLRequestContext()->server_bound_cert_service();
  from_service->GetCertStore()->GetAllServerBoundCerts(
      base::Bind(
          &ProfileAuthDataTransferer::OnServerBoundCertsToTransferRetrieved,
          base::Unretained(this)));
}

void ProfileAuthDataTransferer::OnServerBoundCertsToTransferRetrieved(
    const net::ServerBoundCertStore::ServerBoundCertList& certs_to_transfer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  certs_to_transfer_ = certs_to_transfer;
  got_server_bound_certs_ = true;
  MaybeTransferCookiesAndCerts();
}

void ProfileAuthDataTransferer::MaybeTransferCookiesAndCerts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!(got_cookies_ && got_server_bound_certs_))
    return;

  // Nothing to transfer over?
  if (!cookies_to_transfer_.size()) {
    Finish();
    return;
  }

  // Retrieve the contents of |to_context_|'s cookie jar.
  net::CookieStore* to_store =
      to_context_->GetURLRequestContext()->cookie_store();
  net::CookieMonster* to_monster = to_store->GetCookieMonster();
  to_monster->GetAllCookiesAsync(
      base::Bind(&ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved,
                 base::Unretained(this)));
}

void ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved(
    const net::CookieList& target_cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (target_cookies.empty()) {
    net::CookieStore* to_store =
        to_context_->GetURLRequestContext()->cookie_store();
    net::CookieMonster* to_monster = to_store->GetCookieMonster();
    to_monster->InitializeFrom(cookies_to_transfer_);
    net::ServerBoundCertService* to_cert_service =
        to_context_->GetURLRequestContext()->server_bound_cert_service();
    to_cert_service->GetCertStore()->InitializeFrom(certs_to_transfer_);
  }

  Finish();
}

void ProfileAuthDataTransferer::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!completion_callback_.is_null())
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion_callback_);
  delete this;
}

}  // namespace

void ProfileAuthData::Transfer(
    content::BrowserContext* from_context,
    content::BrowserContext* to_context,
    bool transfer_auth_cookies_and_server_bound_certs,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  (new ProfileAuthDataTransferer(from_context,
                                 to_context,
                                 transfer_auth_cookies_and_server_bound_certs,
                                 completion_callback))->BeginTransfer();
}

}  // namespace chromeos
