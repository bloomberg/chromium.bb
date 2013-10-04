// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_cookie_helper.h"

#include <vector>

#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

SigninManagerCookieHelper::SigninManagerCookieHelper(
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SigninManagerCookieHelper::~SigninManagerCookieHelper() {
}

void SigninManagerCookieHelper::StartFetchingGaiaCookiesOnUIThread(
    const base::Callback<void(const net::CookieList& cookies)>& callback) {
  StartFetchingCookiesOnUIThread(
      GaiaUrls::GetInstance()->gaia_url(), callback);
}

void SigninManagerCookieHelper::StartFetchingCookiesOnUIThread(
    const GURL& url,
    const base::Callback<void(const net::CookieList& cookies)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());

  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::FetchCookiesOnIOThread, this,
                 url));
}

void SigninManagerCookieHelper::FetchCookiesOnIOThread(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<net::CookieMonster> cookie_monster =
      request_context_getter_->GetURLRequestContext()->
      cookie_store()->GetCookieMonster();
  if (cookie_monster.get()) {
    cookie_monster->GetAllCookiesForURLAsync(
        url, base::Bind(&SigninManagerCookieHelper::OnCookiesFetched, this));
  } else {
    OnCookiesFetched(net::CookieList());
  }
}

void SigninManagerCookieHelper::OnCookiesFetched(
    const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::NotifyOnUIThread, this, cookies));
}

void SigninManagerCookieHelper::NotifyOnUIThread(
    const net::CookieList& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ResetAndReturn(&completion_callback_).Run(cookies);
}
