// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_wrapper.h"

// static
BrowserContextKeyedService* FakeProfileOAuth2TokenServiceWrapper::Build(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new FakeProfileOAuth2TokenServiceWrapper(profile, false);
}

// static
BrowserContextKeyedService*
FakeProfileOAuth2TokenServiceWrapper::BuildAutoIssuingTokenService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new FakeProfileOAuth2TokenServiceWrapper(profile, true);
}

FakeProfileOAuth2TokenServiceWrapper::FakeProfileOAuth2TokenServiceWrapper(
    Profile* profile,
    bool auto_issue_tokens) {
  if (auto_issue_tokens)
    service_.set_auto_post_fetch_response_on_message_loop(true);
  service_.Initialize(reinterpret_cast<Profile*>(profile));
}

FakeProfileOAuth2TokenServiceWrapper::~FakeProfileOAuth2TokenServiceWrapper() {
  service_.Shutdown();
}

ProfileOAuth2TokenService*
FakeProfileOAuth2TokenServiceWrapper::GetProfileOAuth2TokenService() {
  return &service_;
}
