// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager_cookie_helper.h"

#include <vector>

#include "base/message_loop/message_loop_proxy.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"

SigninManagerCookieHelper::SigninManagerCookieHelper(
    net::URLRequestContextGetter* request_context_getter,
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    scoped_refptr<base::MessageLoopProxy> io_thread)
    : request_context_getter_(request_context_getter),
      ui_thread_(ui_thread),
      io_thread_(io_thread) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
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
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());

  completion_callback_ = callback;
  io_thread_->PostTask(FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::FetchCookiesOnIOThread,
                 this,
                 url));
}

void SigninManagerCookieHelper::FetchCookiesOnIOThread(const GURL& url) {
  DCHECK(io_thread_->BelongsToCurrentThread());

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
  DCHECK(io_thread_->BelongsToCurrentThread());
  ui_thread_->PostTask(FROM_HERE,
      base::Bind(&SigninManagerCookieHelper::NotifyOnUIThread, this, cookies));
}

void SigninManagerCookieHelper::NotifyOnUIThread(
    const net::CookieList& cookies) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::ResetAndReturn(&completion_callback_).Run(cookies);
}
