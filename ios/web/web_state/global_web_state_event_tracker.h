// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_
#define IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ios/web/public/web_state/global_web_state_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace web {

// This singleton serves as the mechanism via which GlobalWebStateObservers get
// informed of relevant events from all WebState instances.
class GlobalWebStateEventTracker {
 public:
  // Returns the instance of GlobalWebStateEventTracker.
  static GlobalWebStateEventTracker* GetInstance();

  // Adds/removes observers.
  void AddObserver(GlobalWebStateObserver* observer);
  void RemoveObserver(GlobalWebStateObserver* observer);

 private:
  friend struct base::DefaultSingletonTraits<GlobalWebStateEventTracker>;
  friend class WebStateEventForwarder;
  friend class WebStateImpl;

  // Should be called whenever a WebState instance is created.
  void OnWebStateCreated(WebState* web_state);

  // Forward to the registered observers.
  void NavigationItemsPruned(WebState* web_state, size_t pruned_item_count);
  void NavigationItemChanged(WebState* web_state);
  void NavigationItemCommitted(WebState* web_state,
                               const LoadCommittedDetails& load_details);
  void WebStateDidStartLoading(WebState* web_state);
  void WebStateDidStopLoading(WebState* web_state);
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status);
  void RenderProcessGone(WebState* web_state);
  void WebStateDestroyed(WebState* web_state);

  GlobalWebStateEventTracker();
  ~GlobalWebStateEventTracker();

  // List of observers currently registered with the tracker.
  base::ObserverList<GlobalWebStateObserver, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(GlobalWebStateEventTracker);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_GLOBAL_WEB_STATE_EVENT_TRACKER_H_
