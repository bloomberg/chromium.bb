// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROXIMITY_AUTH_FACADE_H_
#define CHROME_BROWSER_SIGNIN_PROXIMITY_AUTH_FACADE_H_

namespace proximity_auth {
class ScreenlockBridge;
}

// Returns the global proximity_auth::ScreenlockBridge instance.
proximity_auth::ScreenlockBridge* GetScreenlockBridgeInstance();

#endif  // CHROME_BROWSER_SIGNIN_PROXIMITY_AUTH_FACADE_H_
