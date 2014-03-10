// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_

// An interface that needs to be supplied to the Signin component by its
// embedder.
class SigninClient {
 public:
  virtual ~SigninClient() {}

  // Returns true if the cookie policy for the execution context of
  // the SigninManager allows cookies for the Google signin domain.
  virtual bool AreSigninCookiesAllowed() = 0;
};

#endif  // COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_
