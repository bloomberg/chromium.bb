// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_SIGNIN_H_
#define CHROME_BROWSER_SYNC_UTIL_SIGNIN_H_

// This enumeration is here since we used to support hosted and non-hosted
// accounts, but now only the latter is supported.
enum SignIn {
  // The account foo@domain is authenticated as a consumer account.
  GMAIL_SIGNIN
};

#endif  // CHROME_BROWSER_SYNC_UTIL_SIGNIN_H_
