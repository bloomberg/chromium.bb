// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
#define CHROMEOS_LOGIN_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_

#include "base/macros.h"

namespace chromeos {

// A class to start and shutdown public session state for a test.
class ScopedTestPublicSessionLoginState {
 public:
  ScopedTestPublicSessionLoginState();
  ~ScopedTestPublicSessionLoginState();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTestPublicSessionLoginState);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_SCOPED_TEST_PUBLIC_SESSION_LOGIN_STATE_H_
