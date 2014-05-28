// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/preconnect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace chromeos {

AuthPrewarmer::AuthPrewarmer()
    : doing_prewarm_(false) {
}

AuthPrewarmer::~AuthPrewarmer() {
  if (registrar_.IsRegistered(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(ProfileHelper::GetSigninProfile()))) {
    registrar_.Remove(
        this,
        chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
        content::Source<Profile>(ProfileHelper::GetSigninProfile()));
  }
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
}

void AuthPrewarmer::PrewarmAuthentication(
    const base::Closure& completion_callback) {
  if (doing_prewarm_) {
    LOG(ERROR) << "PrewarmAuthentication called twice.";
    return;
  }
  doing_prewarm_ = true;
  completion_callback_ = completion_callback;
  if (GetRequestContext() && IsNetworkConnected()) {
    DoPrewarm();
    return;
  }
  if (!IsNetworkConnected()) {
    // DefaultNetworkChanged will get called when a network becomes connected.
    NetworkHandler::Get()->network_state_handler()
        ->AddObserver(this, FROM_HERE);
  }
  if (!GetRequestContext()) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
        content::Source<Profile>(ProfileHelper::GetSigninProfile()));
  }
}

void AuthPrewarmer::DefaultNetworkChanged(const NetworkState* network) {
  if (!network)
    return;  // Still no default (connected) network.

  NetworkHandler::Get()->network_state_handler()
      ->RemoveObserver(this, FROM_HERE);
  if (GetRequestContext())
    DoPrewarm();
}

void AuthPrewarmer::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED:
      registrar_.Remove(
          this,
          chrome::NOTIFICATION_PROFILE_URL_REQUEST_CONTEXT_GETTER_INITIALIZED,
          content::Source<Profile>(ProfileHelper::GetSigninProfile()));
      if (IsNetworkConnected())
        DoPrewarm();
      break;
    default:
      NOTREACHED();
  }
}

void AuthPrewarmer::DoPrewarm() {
  const int kConnectionsNeeded = 1;

  std::vector<GURL> urls;
  urls.push_back(GaiaUrls::GetInstance()->client_login_url());
  urls.push_back(GaiaUrls::GetInstance()->service_login_url());

  for (size_t i = 0; i < urls.size(); ++i) {
    chrome_browser_net::PreconnectOnUIThread(
        urls[i],
        urls[i],
        chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
        kConnectionsNeeded,
        GetRequestContext());
  }
  if (!completion_callback_.is_null()) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     completion_callback_);
  }
}

bool AuthPrewarmer::IsNetworkConnected() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return (nsh->ConnectedNetworkByType(NetworkTypePattern::Default()) != NULL);
}

net::URLRequestContextGetter* AuthPrewarmer::GetRequestContext() const {
  return ProfileHelper::GetSigninProfile()->GetRequestContext();
}

}  // namespace chromeos
