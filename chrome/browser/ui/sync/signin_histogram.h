// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_SIGNIN_HISTOGRAM_H_
#define CHROME_BROWSER_UI_SYNC_SIGNIN_HISTOGRAM_H_

namespace signin {

// Enum values used for use with the "Signin.Reauth" histogram.
enum account_reauth{
  // The user gave the wrong email when doing a reauth.
  HISTOGRAM_ACCOUNT_MISSMATCH,

  // The user given a reauth login screen.
  HISTOGRAM_SHOWN,

  HISTOGRAM_MAX
};

}

#endif  // CHROME_BROWSER_UI_SYNC_SIGNIN_HISTOGRAM_H_
