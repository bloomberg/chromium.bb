// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"

std::unique_ptr<KeyedService> BuildFakeProfileOAuth2TokenService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<FakeProfileOAuth2TokenService> service(
      new FakeProfileOAuth2TokenService(profile->GetPrefs()));
  return std::move(service);
}

std::unique_ptr<KeyedService> BuildAutoIssuingFakeProfileOAuth2TokenService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<FakeProfileOAuth2TokenService> service(
      new FakeProfileOAuth2TokenService(profile->GetPrefs()));
  service->set_auto_post_fetch_response_on_message_loop(true);
  return std::move(service);
}
