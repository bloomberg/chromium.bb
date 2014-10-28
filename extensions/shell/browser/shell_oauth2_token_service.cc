// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_oauth2_token_service.h"

#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

namespace extensions {
namespace {

ShellOAuth2TokenService* g_instance = nullptr;

}  // namespace

ShellOAuth2TokenService::ShellOAuth2TokenService(
    content::BrowserContext* browser_context,
    std::string account_id,
    std::string refresh_token)
    : browser_context_(browser_context),
      account_id_(account_id),
      refresh_token_(refresh_token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!g_instance);
  g_instance = this;
}

ShellOAuth2TokenService::~ShellOAuth2TokenService() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(g_instance);
  g_instance = nullptr;
}

// static
ShellOAuth2TokenService* ShellOAuth2TokenService::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

void ShellOAuth2TokenService::SetRefreshToken(
    const std::string& account_id,
    const std::string& refresh_token) {
  account_id_ = account_id;
  refresh_token_ = refresh_token;
}

bool ShellOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  if (account_id != account_id_)
    return false;

  return !refresh_token_.empty();
}

OAuth2AccessTokenFetcher* ShellOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK(!refresh_token_.empty());
  return new OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token_);
}

net::URLRequestContextGetter* ShellOAuth2TokenService::GetRequestContext() {
  return browser_context_->GetRequestContext();
}

}  // namespace extensions
