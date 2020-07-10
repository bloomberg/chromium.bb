// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AuthenticationServiceDelegateFake::AuthenticationServiceDelegateFake() =
    default;

AuthenticationServiceDelegateFake::~AuthenticationServiceDelegateFake() =
    default;

void AuthenticationServiceDelegateFake::ClearBrowsingData(
    ProceduralBlock completion) {
  NOTREACHED();
}
