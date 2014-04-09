// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROFILE_MOCK_H_
#define CHROME_TEST_BASE_PROFILE_MOCK_H_

#include "chrome/test/base/testing_profile.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {
class PasswordStore;
}

class ProfileMock : public TestingProfile {
 public:
  ProfileMock();
  virtual ~ProfileMock();

  MOCK_METHOD1(GetPasswordStore,
               password_manager::PasswordStore*(ServiceAccessType access));
};

#endif  // CHROME_TEST_BASE_PROFILE_MOCK_H_
