// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace chromeos {

namespace {

class ProfileAuthDataTransferer {
 public:
  ProfileAuthDataTransferer(
      Profile* from_profile,
      Profile* to_profile,
      bool transfer_cookies,
      const base::Closure& completion_callback);

  void BeginTransfer();

 private:
  void BeginTransferOnIOThread();
  void MaybeDoCookieAndCertTransfer();
  void Finish();

  void OnTransferCookiesIfEmptyJar(const net::CookieList& cookies_in_jar);
  void OnGetCookiesToTransfer(const net::CookieList& cookies_to_transfer);
  void RetrieveDefaultCookies();
  void OnGetServerBoundCertsToTransfer(
      const net::ServerBoundCertStore::ServerBoundCertList& certs);
  void RetrieveDefaultServerBoundCerts();
  void TransferDefaultAuthCache();

  scoped_refptr<net::URLRequestContextGetter> from_context_;
  scoped_refptr<net::URLRequestContextGetter> to_context_;
  bool transfer_cookies_;
  base::Closure completion_callback_;

  net::CookieList cookies_to_transfer_;
  net::ServerBoundCertStore::ServerBoundCertList certs_to_transfer_;

  bool got_cookies_;
  bool got_server_bound_certs_;
};

ProfileAuthDataTransferer::ProfileAuthDataTransferer(
    Profile* from_profile,
    Profile* to_profile,
    bool transfer_cookies,
    const base::Closure& completion_callback)
    : from_context_(from_profile->GetRequestContext()),
      to_context_(to_profile->GetRequestContext()),
      transfer_cookies_(transfer_cookies),
      completion_callback_(completion_callback),
      got_cookies_(false),
      got_server_bound_certs_(false) {
}

void ProfileAuthDataTransferer::BeginTransfer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we aren't transferring cookies, post the completion callback
  // immediately.  Otherwise, it will be called when both cookies and channel
  // ids are finished transferring.
  if (!transfer_cookies_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion_callback_);
    // Null the callback so that when Finish is called the callback won't be
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
  TransferDefaultAuthCache();

  if (transfer_cookies_) {
    RetrieveDefaultCookies();
    RetrieveDefaultServerBoundCerts();
  } else {
    Finish();
  }
}

// If both cookies and server bound certs have been retrieved, see if we need to
// do the actual transfer.
void ProfileAuthDataTransferer::MaybeDoCookieAndCertTransfer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!(got_cookies_ && got_server_bound_certs_))
    return;

  // Nothing to transfer over?
  if (!cookies_to_transfer_.size()) {
    Finish();
    return;
  }

  // Now let's see if the target cookie monster's jar is even empty.
  net::CookieStore* to_store =
      to_context_->GetURLRequestContext()->cookie_store();
  net::CookieMonster* to_monster = to_store->GetCookieMonster();
  to_monster->GetAllCookiesAsync(
      base::Bind(&ProfileAuthDataTransferer::OnTransferCookiesIfEmptyJar,
                 base::Unretained(this)));
}

// Post the |completion_callback_| and delete ourself.
void ProfileAuthDataTransferer::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!completion_callback_.is_null())
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion_callback_);
  delete this;
}

// Callback for transferring |cookies_to_transfer_| into |to_context_|'s
// CookieMonster if its jar is completely empty.  If authentication was
// performed by an extension, then the set of cookies that was acquired through
// such that process will be automatically transfered into the profile.
void ProfileAuthDataTransferer::OnTransferCookiesIfEmptyJar(
    const net::CookieList& cookies_in_jar) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Transfer only if the existing cookie jar is empty.
  if (!cookies_in_jar.size()) {
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

// Callback for receiving |cookies_to_transfer| from the authentication profile
// cookie jar.
void ProfileAuthDataTransferer::OnGetCookiesToTransfer(
    const net::CookieList& cookies_to_transfer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  got_cookies_ = true;
  cookies_to_transfer_ = cookies_to_transfer;
  MaybeDoCookieAndCertTransfer();
}

// Retrieves initial set of Profile cookies from the |from_context_|.
void ProfileAuthDataTransferer::RetrieveDefaultCookies() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::CookieStore* from_store =
      from_context_->GetURLRequestContext()->cookie_store();
  net::CookieMonster* from_monster = from_store->GetCookieMonster();
  from_monster->SetKeepExpiredCookies();
  from_monster->GetAllCookiesAsync(
      base::Bind(&ProfileAuthDataTransferer::OnGetCookiesToTransfer,
                 base::Unretained(this)));
}

// Callback for receiving |cookies_to_transfer| from the authentication profile
// cookie jar.
void ProfileAuthDataTransferer::OnGetServerBoundCertsToTransfer(
    const net::ServerBoundCertStore::ServerBoundCertList& certs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  certs_to_transfer_ = certs;
  got_server_bound_certs_ = true;
  MaybeDoCookieAndCertTransfer();
}

// Retrieves server bound certs of |from_context_|.
void ProfileAuthDataTransferer::RetrieveDefaultServerBoundCerts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::ServerBoundCertService* from_service =
      from_context_->GetURLRequestContext()->server_bound_cert_service();

  from_service->GetCertStore()->GetAllServerBoundCerts(
      base::Bind(&ProfileAuthDataTransferer::OnGetServerBoundCertsToTransfer,
                 base::Unretained(this)));
}

// Transfers HTTP authentication cache from the |from_context_|
// into the |to_context_|. If user was required to authenticate with a proxy
// during the login, this authentication information will be transferred
// into the new session.
void ProfileAuthDataTransferer::TransferDefaultAuthCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HttpAuthCache* new_cache = to_context_->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  new_cache->UpdateAllFrom(*from_context_->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache());
}

}  // namespace

void ProfileAuthData::Transfer(
    Profile* from_profile,
    Profile* to_profile,
    bool transfer_cookies,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  (new ProfileAuthDataTransferer(from_profile, to_profile, transfer_cookies,
                                 completion_callback))->BeginTransfer();
}

}  // namespace chromeos
