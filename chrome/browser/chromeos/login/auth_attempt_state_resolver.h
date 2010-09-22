// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_RESOLVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_RESOLVER_H_
#pragma once

namespace chromeos {

class AuthAttemptStateResolver {
 public:
  AuthAttemptStateResolver();
  virtual ~AuthAttemptStateResolver();
  // Gather existing status info and attempt to resolve it into one of a
  // set of discrete states.
  virtual void Resolve() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_ATTEMPT_STATE_RESOLVER_H_
