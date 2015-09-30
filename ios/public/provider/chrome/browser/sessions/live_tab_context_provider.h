// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_PROVIDER_H_

#include "base/basictypes.h"

namespace sessions {
class LiveTab;
class LiveTabContext;
}

namespace ios {

class ChromeBrowserState;

// A class that allows retrieving a sessions::LiveTabContext.
class LiveTabContextProvider {
 public:
  virtual ~LiveTabContextProvider() {}

  // Creates a new context.
  virtual sessions::LiveTabContext* Create(
      ios::ChromeBrowserState* browser_state) = 0;

  // Retrieves the context with the given ID, if one exists.
  virtual sessions::LiveTabContext* FindContextWithID(int32 desired_id) = 0;

  // Retrieves the context for the given tab, if one exists.
  virtual sessions::LiveTabContext* FindContextForTab(
      const sessions::LiveTab* tab) = 0;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_LIVE_TAB_CONTEXT_PROVIDER_H_
