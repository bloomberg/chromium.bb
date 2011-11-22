// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"

namespace chromeos {

MockCryptohomeLibrary::MockCryptohomeLibrary()
    : outcome_(false), code_(0) {
}

MockCryptohomeLibrary::~MockCryptohomeLibrary() {}

void MockCryptohomeLibrary::SetUp(bool outcome, int code) {
  outcome_ = outcome;
  code_ = code;
  ON_CALL(*this, AsyncCheckKey(_, _, _))
      .WillByDefault(
          WithArgs<2>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  ON_CALL(*this, AsyncMigrateKey(_, _, _, _))
      .WillByDefault(
          WithArgs<3>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  ON_CALL(*this, AsyncMount(_, _, _, _))
      .WillByDefault(
          WithArgs<3>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  ON_CALL(*this, AsyncMountForBwsi(_))
      .WillByDefault(
          WithArgs<0>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  ON_CALL(*this, AsyncRemove(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
  ON_CALL(*this, AsyncSetOwnerUser(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(this, &MockCryptohomeLibrary::DoCallback)));
}

}  // namespace chromeos
