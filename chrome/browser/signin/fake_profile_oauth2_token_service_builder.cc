// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"

// TODO(blundell): Should these be namespaced?
KeyedService* BuildFakeProfileOAuth2TokenService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeProfileOAuth2TokenService* service = new FakeProfileOAuth2TokenService();
  service->Initialize(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile));
  return service;
}

KeyedService* BuildAutoIssuingFakeProfileOAuth2TokenService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeProfileOAuth2TokenService* service = new FakeProfileOAuth2TokenService();
  service->set_auto_post_fetch_response_on_message_loop(true);
  service->Initialize(
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile));
  return service;
}
