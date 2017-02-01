// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_

#include <stddef.h>

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"

namespace web {

struct LoadCommittedDetails;
class WebState;

// An observer API implemented by classes that are interested in various
// WebState-related events from all WebStates. GlobalWebStateObservers will
// receive callbacks for the duration of their lifetime.
// WARNING: This class exists only to mimic //chrome-specific flows that
// listen for notifications from all WebContents or NavigationController
// instances. Do not add new subclasses of this class for any other reason.
class GlobalWebStateObserver {
 public:
  // Called when navigation items have been pruned in |web_state|.
  virtual void NavigationItemsPruned(WebState* web_state,
                                     size_t pruned_item_count) {}

  // Called when a navigation item has changed in |web_state|.
  virtual void NavigationItemChanged(WebState* web_state) {}

  // Called when a navigation item has been committed in |web_state|.
  virtual void NavigationItemCommitted(
      WebState* web_state,
      const LoadCommittedDetails& load_details) {}

  // Called when |web_state| has started loading a page.
  virtual void WebStateDidStartLoading(WebState* web_state) {}

  // Called when |web_state| has stopped loading a page.
  virtual void WebStateDidStopLoading(WebState* web_state) {}

  // Called when the current page is loaded in |web_state|.
  virtual void PageLoaded(WebState* web_state,
                          PageLoadCompletionStatus load_completion_status) {}

  // Called when the web process is terminated (usually by crashing, though
  // possibly by other means).
  virtual void RenderProcessGone(WebState* web_state) {}

  // Called when |web_state| is being destroyed.
  virtual void WebStateDestroyed(WebState* web_state) {}

 protected:
  GlobalWebStateObserver();
  virtual ~GlobalWebStateObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalWebStateObserver);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_GLOBAL_WEB_STATE_OBSERVER_H_
