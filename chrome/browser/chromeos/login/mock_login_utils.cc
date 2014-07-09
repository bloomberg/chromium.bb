// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_login_utils.h"

#include "chromeos/login/auth/user_context.h"

using namespace testing;

namespace chromeos {

MockLoginUtils::MockLoginUtils() {}

MockLoginUtils::~MockLoginUtils() {}

void MockLoginUtils::DelegateToFake() {
  if (fake_login_utils_.get())
    return;
  fake_login_utils_.reset(new FakeLoginUtils());
  FakeLoginUtils* fake = fake_login_utils_.get();
  ON_CALL(*this, DoBrowserLaunch(_, _))
      .WillByDefault(Invoke(fake, &FakeLoginUtils::DoBrowserLaunch));
  ON_CALL(*this, PrepareProfile(_, _, _, _))
      .WillByDefault(Invoke(fake, &FakeLoginUtils::PrepareProfile));
  ON_CALL(*this, CreateAuthenticator(_))
      .WillByDefault(Invoke(fake, &FakeLoginUtils::CreateAuthenticator));
}

FakeLoginUtils* MockLoginUtils::GetFakeLoginUtils() {
  return fake_login_utils_.get();
}

}  // namespace chromeos
