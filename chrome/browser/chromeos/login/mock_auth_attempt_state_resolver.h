// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_ATTEMPT_STATE_RESOLVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_ATTEMPT_STATE_RESOLVER_H_
#pragma once

#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockAuthAttemptStateResolver : public AuthAttemptStateResolver {
 public:
  MockAuthAttemptStateResolver() {}
  virtual ~MockAuthAttemptStateResolver() {}
  MOCK_METHOD0(Resolve, void(void));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockAuthAttemptStateResolver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_ATTEMPT_STATE_RESOLVER_H_
