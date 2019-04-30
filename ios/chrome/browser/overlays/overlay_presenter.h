// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_

#import "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class OverlayUIDelegate;

// OverlayPresenter is responsible for triggering the presentation of overlay UI
// at a given modality.  It observes OverlayRequestQueue modifications for the
// active WebState and triggers the presentation for added requests using the UI
// delegate.  Additionally, the presenter will manage hiding and showing
// overlays when the foreground tab is updated.
class OverlayPresenter : public OverlayRequestQueueImplObserver,
                         public WebStateListObserver {
 public:
  OverlayPresenter(OverlayModality modality, WebStateList* web_state_list);
  ~OverlayPresenter() override;

  // Setter for the UI delegate.  The presenter will begin trying to present
  // overlay UI when the delegate is set.
  void SetUIDelegate(OverlayUIDelegate* ui_delegate);

  // Disconnects observers in preparation of shutdown.  Must be called before
  // destruction.
  void Disconnect();

 private:
  // Fetches the request queue for |web_state|, creating it if necessary.
  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state);

  // Starts and stops observing |web_state_list_| and its WebStates request
  // queues.
  void StartObserving();
  void StopObserving();

  // OverlayRequestQueueImplObserver:
  void OnRequestAdded(OverlayRequestQueueImpl* queue,
                      OverlayRequest* request) override;
  void OnRequestRemoved(OverlayRequestQueueImpl* queue,
                        OverlayRequest* request,
                        bool frontmost) override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WillDetachWebStateAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) override;

  OverlayModality modality_;
  WebStateList* web_state_list_ = nullptr;
  OverlayUIDelegate* ui_delegate_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_
