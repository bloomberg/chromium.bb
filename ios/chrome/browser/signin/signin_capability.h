// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CAPABILITY_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CAPABILITY_H_

namespace ios {
class ChromeBrowserState;
}

// Whether an Auto-Login prompt should be presented to the user.
bool CanPresentAutoLogin(ios::ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_CAPABILITY_H_
