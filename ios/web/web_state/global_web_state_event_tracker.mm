// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/global_web_state_event_tracker.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace web {

class WebStateEventForwarder;
DEFINE_WEB_STATE_USER_DATA_KEY(WebStateEventForwarder);

// Helper class that forwards events from a single associated WebState instance
// to the GlobalWebStateEventTracker singleton.
class WebStateEventForwarder : public WebStateUserData<WebStateEventForwarder>,
                               public WebStateObserver {
 public:
  ~WebStateEventForwarder() override;

 private:
  explicit WebStateEventForwarder(WebState* web_state);
  friend class WebStateUserData<WebStateEventForwarder>;

  // WebContentsObserver:
  void NavigationItemsPruned(size_t pruned_item_count) override;
  void NavigationItemChanged() override;
  void NavigationItemCommitted(
      const LoadCommittedDetails& load_details) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void PageLoaded(PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed() override;

  DISALLOW_COPY_AND_ASSIGN(WebStateEventForwarder);
};

#pragma mark - WebStateEventForwarder

WebStateEventForwarder::WebStateEventForwarder(web::WebState* web_state)
    : WebStateObserver(web_state) {}

WebStateEventForwarder::~WebStateEventForwarder() {}

void WebStateEventForwarder::NavigationItemsPruned(size_t pruned_item_count) {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemsPruned(
      web_state(), pruned_item_count);
}

void WebStateEventForwarder::NavigationItemChanged() {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemChanged(web_state());
}

void WebStateEventForwarder::NavigationItemCommitted(
    const LoadCommittedDetails& load_details) {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemCommitted(
      web_state(), load_details);
}

void WebStateEventForwarder::DidStartLoading() {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStartLoading(
      web_state());
}

void WebStateEventForwarder::DidStopLoading() {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStopLoading(
      web_state());
}

void WebStateEventForwarder::PageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  GlobalWebStateEventTracker::GetInstance()->PageLoaded(web_state(),
                                                        load_completion_status);
}

void WebStateEventForwarder::WebStateDestroyed() {
  GlobalWebStateEventTracker::GetInstance()->WebStateDestroyed(web_state());
}

#pragma mark - GlobalWebStateEventTracker

GlobalWebStateEventTracker* GlobalWebStateEventTracker::GetInstance() {
  return base::Singleton<GlobalWebStateEventTracker>::get();
}

GlobalWebStateEventTracker::GlobalWebStateEventTracker() {}

GlobalWebStateEventTracker::~GlobalWebStateEventTracker() {}

void GlobalWebStateEventTracker::OnWebStateCreated(WebState* web_state) {
  WebStateEventForwarder::CreateForWebState(web_state);
}

void GlobalWebStateEventTracker::AddObserver(GlobalWebStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GlobalWebStateEventTracker::RemoveObserver(
    GlobalWebStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GlobalWebStateEventTracker::NavigationItemsPruned(
    WebState* web_state,
    size_t pruned_item_count) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    NavigationItemsPruned(web_state, pruned_item_count));
}

void GlobalWebStateEventTracker::NavigationItemChanged(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    NavigationItemChanged(web_state));
}

void GlobalWebStateEventTracker::NavigationItemCommitted(
    WebState* web_state,
    const LoadCommittedDetails& load_details) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    NavigationItemCommitted(web_state, load_details));
}

void GlobalWebStateEventTracker::WebStateDidStartLoading(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    WebStateDidStartLoading(web_state));
}

void GlobalWebStateEventTracker::WebStateDidStopLoading(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    WebStateDidStopLoading(web_state));
}

void GlobalWebStateEventTracker::PageLoaded(
    WebState* web_state,
    PageLoadCompletionStatus load_completion_status) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    PageLoaded(web_state, load_completion_status));
}

void GlobalWebStateEventTracker::WebStateDestroyed(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    WebStateDestroyed(web_state));
}

}  // namespace web
