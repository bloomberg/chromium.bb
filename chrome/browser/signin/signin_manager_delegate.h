// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_

// A delegate interface that needs to be supplied to SigninManager by
// the embedder.
class SigninManagerDelegate {
 public:
  virtual ~SigninManagerDelegate() {}

  // Returns true if the cookie policy for the execution context of
  // the SigninManager allows cookies for the Google signin domain.
  virtual bool AreSigninCookiesAllowed() = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_DELEGATE_H_
