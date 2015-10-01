// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/global_web_state_event_tracker.h"

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
  void DidStartLoading() override;
  void DidStopLoading() override;

  DISALLOW_COPY_AND_ASSIGN(WebStateEventForwarder);
};

#pragma mark - WebStateEventForwarder

WebStateEventForwarder::WebStateEventForwarder(web::WebState* web_state)
    : WebStateObserver(web_state) {}

WebStateEventForwarder::~WebStateEventForwarder() {}

void WebStateEventForwarder::DidStartLoading() {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStartLoading(
      web_state());
}

void WebStateEventForwarder::DidStopLoading() {
  GlobalWebStateEventTracker::GetInstance()->WebStateDidStopLoading(
      web_state());
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

void GlobalWebStateEventTracker::WebStateDidStartLoading(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    WebStateDidStartLoading(web_state));
}

void GlobalWebStateEventTracker::WebStateDidStopLoading(WebState* web_state) {
  FOR_EACH_OBSERVER(GlobalWebStateObserver, observer_list_,
                    WebStateDidStopLoading(web_state));
}

}  // namespace web
