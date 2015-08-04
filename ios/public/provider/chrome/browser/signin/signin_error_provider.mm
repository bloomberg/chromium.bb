// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"

namespace ios {

namespace {
SigninErrorProvider* g_signin_error_provider = nullptr;
}

void SetSigninErrorProvider(SigninErrorProvider* provider) {
  g_signin_error_provider = provider;
}

SigninErrorProvider* GetSigninErrorProvider() {
  return g_signin_error_provider;
}

SigninErrorProvider::SigninErrorProvider() {}

SigninErrorProvider::~SigninErrorProvider() {}

SigninErrorCategory SigninErrorProvider::GetErrorCategory(NSError* error) {
  return SigninErrorCategory::UNKNOWN_ERROR;
}

bool SigninErrorProvider::IsCanceled(NSError* error) {
  return false;
}

NSString* SigninErrorProvider::GetInvalidGrantJsonErrorKey() {
  return @"invalid_grant_error_key";
}

NSString* SigninErrorProvider::GetSigninErrorDomain() {
  return @"signin_error_domain";
}

int SigninErrorProvider::GetCode(SigninError error) {
  return 0;
}

}  // namesace ios
