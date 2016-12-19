// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_

namespace web {
class BrowserState;
}

// Utility method that allows to access the iOS SessionService from C++ code.
namespace session_util {

// Deletes the file containing the commands for the last session. Finishes the
// deletion even if |browser_state| is destroyed after this call.
void DeleteLastSession(web::BrowserState* browser_state);

}  // namespace session_util

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_
