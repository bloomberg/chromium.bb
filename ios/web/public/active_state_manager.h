// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_ACTIVE_STATE_MANAGER_H_
#define IOS_WEB_PUBLIC_ACTIVE_STATE_MANAGER_H_

#include "base/macros.h"

namespace web {

class BrowserState;

// Manages the active state associated with a particular BrowserState. Not
// thread safe. Must be used only on the main thread.
class ActiveStateManager {
 public:
  // Sets the active state of the ActiveStateManager. At most one
  // ActiveStateManager can be active at any given time in the app. A
  // ActiveStateManager must be made inactive before it is destroyed. It is
  // valid to call |SetActive(true)| on an already active ActiveStateManager.
  virtual void SetActive(bool active) = 0;
  // Returns true if the BrowserState is active.
  virtual bool IsActive() = 0;

 protected:
  virtual ~ActiveStateManager(){};
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_ACTIVE_STATE_MANAGER_H_
