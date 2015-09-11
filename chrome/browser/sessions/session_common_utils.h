// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_

class GURL;

// Returns whether sessions code should track a URL for restoring in the context
// of //chrome. In particular, blacklists chrome://quit and chrome://restart to
// avoid quit or restart loops.
bool ShouldTrackURLForRestore(const GURL& url);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_
