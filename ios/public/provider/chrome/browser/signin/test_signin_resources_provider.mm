// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/test_signin_resources_provider.h"

#import <UIKit/UIKit.h>

TestSigninResourcesProvider::TestSigninResourcesProvider()
    : default_avatar_([[UIImage alloc] init]) {}

TestSigninResourcesProvider::~TestSigninResourcesProvider() {}

UIImage* TestSigninResourcesProvider::GetDefaultAvatar() {
  return default_avatar_;
}

NSString* TestSigninResourcesProvider::GetLocalizedString(
    ios::SigninStringID string_id) {
  return @"Test";
}
