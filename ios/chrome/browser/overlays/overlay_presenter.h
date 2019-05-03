// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_ui_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class OverlayUIDelegate;

// OverlayPresenter is responsible for triggering the presentation of overlay UI
// at a given modality.  The presenter:
// - observes OverlayRequestQueue modifications for the active WebState and
//   triggers the presentation for added requests using the UI delegate.
// - manages hiding and showing overlays for active WebState changes.
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
  // Setter for the active WebState.  Setting to a new value will hide any
  // presented overlays and show the next overlay for the new active WebState.
  void SetActiveWebState(web::WebState* web_state,
                         WebStateListObserver::ChangeReason reason);

  // Fetches the request queue for |web_state|, creating it if necessary.
  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) const;

  // Returns the request queue for the active WebState.
  OverlayRequestQueueImpl* GetActiveQueue() const;

  // Returns the front request for the active queue.
  OverlayRequest* GetActiveRequest() const;

  // Triggers the presentation of the overlay UI for the active request.  Does
  // nothing if there is no active request or if there is no UI delegate.  Must
  // only be called when |presenting_| is false.
  void PresentOverlayForActiveRequest();

  // Notifies this object that |ui_delegate| has finished dismissing the
  // overlay UI corresponding with |request| in |queue| for |reason|.  This
  // function is called when the OverlayDismissalCallback provided to the UI
  // delegate is excuted.
  void OverlayWasDismissed(OverlayUIDelegate* ui_delegate,
                           OverlayRequest* request,
                           OverlayRequestQueueImpl* queue,
                           OverlayDismissalReason reason);

  // Cancels all overlays for |web_state|.
  void CancelOverlayUIForWebState(web::WebState* web_state);

  // Cancels all overlays for the Browser.
  void CancelAllOverlayUI();

  // OverlayRequestQueueImplObserver:
  void OnRequestAdded(OverlayRequestQueueImpl* queue,
                      OverlayRequest* request) override;

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
  web::WebState* active_web_state_ = nullptr;
  OverlayUIDelegate* ui_delegate_ = nullptr;
  bool presenting_ = false;
  bool detaching_active_web_state_ = false;
  base::WeakPtrFactory<OverlayPresenter> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_H_
