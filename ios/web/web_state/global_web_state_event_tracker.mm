// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/global_web_state_event_tracker.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  void NavigationItemsPruned(WebState* web_state,
                             size_t pruned_item_count) override;
  void NavigationItemChanged(WebState* web_state) override;
  void NavigationItemCommitted(
      WebState* web_state,
      const LoadCommittedDetails& load_details) override;
  void DidStartLoading(WebState* web_state) override;
  void DidStopLoading(WebState* web_state) override;
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override;
  void RenderProcessGone(WebState* web_state) override;
  void WebStateDestroyed(WebState* web_state) override;

  DISALLOW_COPY_AND_ASSIGN(WebStateEventForwarder);
};

#pragma mark - WebStateEventForwarder

WebStateEventForwarder::WebStateEventForwarder(web::WebState* web_state)
    : WebStateObserver(web_state) {}

WebStateEventForwarder::~WebStateEventForwarder() {}

void WebStateEventForwarder::NavigationItemsPruned(WebState* web_state,
                                                   size_t pruned_item_count) {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemsPruned(
      web_state, pruned_item_count);
}

void WebStateEventForwarder::NavigationItemChanged(WebState* web_state) {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemChanged(web_state);
}

void WebStateEventForwarder::NavigationItemCommitted(
    WebState* web_state,
    const LoadCommittedDetails& load_details) {
  GlobalWebStateEventTracker::GetInstance()->NavigationItemCommitted(
      web_state, load_details);
}

void WebStateEventForwarder::DidStartLoading(WebState* web_state) {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStartLoading(web_state);
}

void WebStateEventForwarder::DidStopLoading(WebState* web_state) {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStopLoading(web_state);
}

void WebStateEventForwarder::PageLoaded(
    WebState* web_state,
    PageLoadCompletionStatus load_completion_status) {
  GlobalWebStateEventTracker::GetInstance()->PageLoaded(web_state,
                                                        load_completion_status);
}

void WebStateEventForwarder::RenderProcessGone(WebState* web_state) {
  GlobalWebStateEventTracker::GetInstance()->RenderProcessGone(web_state);
}

void WebStateEventForwarder::WebStateDestroyed(WebState* web_state) {
  GlobalWebStateEventTracker::GetInstance()->WebStateDestroyed(web_state);
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
  for (auto& observer : observer_list_)
    observer.NavigationItemsPruned(web_state, pruned_item_count);
}

void GlobalWebStateEventTracker::NavigationItemChanged(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.NavigationItemChanged(web_state);
}

void GlobalWebStateEventTracker::NavigationItemCommitted(
    WebState* web_state,
    const LoadCommittedDetails& load_details) {
  for (auto& observer : observer_list_)
    observer.NavigationItemCommitted(web_state, load_details);
}

void GlobalWebStateEventTracker::WebStateDidStartLoading(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDidStartLoading(web_state);
}

void GlobalWebStateEventTracker::WebStateDidStopLoading(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDidStopLoading(web_state);
}

void GlobalWebStateEventTracker::PageLoaded(
    WebState* web_state,
    PageLoadCompletionStatus load_completion_status) {
  for (auto& observer : observer_list_)
    observer.PageLoaded(web_state, load_completion_status);
}

void GlobalWebStateEventTracker::RenderProcessGone(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.RenderProcessGone(web_state);
}

void GlobalWebStateEventTracker::WebStateDestroyed(WebState* web_state) {
  for (auto& observer : observer_list_)
    observer.WebStateDestroyed(web_state);
}

}  // namespace web
