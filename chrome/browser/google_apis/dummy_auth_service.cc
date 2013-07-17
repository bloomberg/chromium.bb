// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/dummy_auth_service.h"

namespace google_apis {

DummyAuthService::DummyAuthService() : dummy_token("testtoken") {
}

void DummyAuthService::AddObserver(AuthServiceObserver* observer) {
}

void DummyAuthService::RemoveObserver(AuthServiceObserver* observer) {
}

void DummyAuthService::StartAuthentication(const AuthStatusCallback& callback) {
}

bool DummyAuthService::HasAccessToken() const {
  return true;
}

bool DummyAuthService::HasRefreshToken() const {
  return true;
}

const std::string& DummyAuthService::access_token() const {
  return dummy_token;
}

void DummyAuthService::ClearAccessToken() {
}

void DummyAuthService::ClearRefreshToken() {
}

}  // namespace google_apis
