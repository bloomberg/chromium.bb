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

// Transfers initial set of Profile cookies from the |from_context| to cookie
// jar of |to_context|.
void TransferDefaultCookiesOnIOThread(
    net::URLRequestContextGetter* from_context,
    net::URLRequestContextGetter* to_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* new_store =
      to_context->GetURLRequestContext()->cookie_store();
  net::CookieMonster* new_monster = new_store->GetCookieMonster();

  net::CookieStore* default_store =
      from_context->GetURLRequestContext()->cookie_store();
  net::CookieMonster* default_monster = default_store->GetCookieMonster();
  default_monster->SetKeepExpiredCookies();
  default_monster->GetAllCookiesAsync(
      base::Bind(base::IgnoreResult(&net::CookieMonster::InitializeFrom),
                 new_monster));
}

// Transfers default server bound certs of |from_context| to server bound certs
// storage of |to_context|.
void TransferDefaultServerBoundCertsIOThread(
    net::URLRequestContextGetter* from_context,
    net::URLRequestContextGetter* to_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::ServerBoundCertService* default_service =
      from_context->GetURLRequestContext()->server_bound_cert_service();

  net::ServerBoundCertStore::ServerBoundCertList server_bound_certs;
  default_service->GetCertStore()->GetAllServerBoundCerts(&server_bound_certs);

  net::ServerBoundCertService* new_service =
      to_context->GetURLRequestContext()->server_bound_cert_service();
  new_service->GetCertStore()->InitializeFrom(server_bound_certs);
}

// Transfers default auth cache of |from_context| to auth cache storage of
// |to_context|.
void TransferDefaultAuthCacheOnIOThread(
    net::URLRequestContextGetter* from_context,
    net::URLRequestContextGetter* to_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HttpAuthCache* new_cache = to_context->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  new_cache->UpdateAllFrom(*from_context->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache());
}

// Transfers cookies and server bound certs from the |from_profile| into
// the |to_profile|.  If authentication was performed by an extension, then
// the set of cookies that was acquired through such that process will be
// automatically transfered into the profile.
void TransferDefaultCookiesAndServerBoundCerts(
    Profile* from_profile,
    Profile* to_profile) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TransferDefaultCookiesOnIOThread,
                 make_scoped_refptr(from_profile->GetRequestContext()),
                 make_scoped_refptr(to_profile->GetRequestContext())));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TransferDefaultServerBoundCertsIOThread,
                 make_scoped_refptr(from_profile->GetRequestContext()),
                 make_scoped_refptr(to_profile->GetRequestContext())));
}

// Transfers HTTP authentication cache from the |from_profile|
// into the |to_profile|. If user was required to authenticate with a proxy
// during the login, this authentication information will be transferred
// into the new session.
void TransferDefaultAuthCache(Profile* from_profile,
                              Profile* to_profile) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TransferDefaultAuthCacheOnIOThread,
                 make_scoped_refptr(from_profile->GetRequestContext()),
                 make_scoped_refptr(to_profile->GetRequestContext())));
}

}  // namespace

void ProfileAuthData::Transfer(Profile* from_profile,
                               Profile* to_profile,
                               bool transfer_cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (transfer_cookies)
    TransferDefaultCookiesAndServerBoundCerts(from_profile, to_profile);

  TransferDefaultAuthCache(from_profile, to_profile);
}
}  // namespace chromeos
