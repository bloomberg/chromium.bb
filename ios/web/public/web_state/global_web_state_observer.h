// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_

#include "base/macros.h"

namespace web {

class WebState;

// An observer API implemented by classes that are interested in various
// WebState-related events from all WebStates. GlobalWebStateObservers will
// receive callbacks for the duration of their lifetime.
// WARNING: This class exists only to mimic //chrome-specific flows that
// listen for notifications from all WebContents or NavigationController
// instances. Do not add new subclasses of this class for any other reason.
class GlobalWebStateObserver {
 public:
  // Called when |web_state| has started loading a page.
  virtual void WebStateDidStartLoading(WebState* web_state) {}

  // Called when |web_state| has stopped loading a page.
  virtual void WebStateDidStopLoading(WebState* web_state) {}

 protected:
  GlobalWebStateObserver();
  virtual ~GlobalWebStateObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalWebStateObserver);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_
