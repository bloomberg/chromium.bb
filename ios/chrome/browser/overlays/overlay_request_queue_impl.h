// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/overlays/overlay_modality.h"
#import "ios/chrome/browser/overlays/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

// Mutable implementation of OverlayRequestQueue.
class OverlayRequestQueueImpl : public OverlayRequestQueue {
 public:
  ~OverlayRequestQueueImpl() override;

  // Container that stores the queues for each modality.  Usage example:
  //
  // OverlayRequestQueueImpl::Container::FromWebState(web_state)->
  //     QueueForModality(OverlayModality::kWebContentArea);
  class Container : public web::WebStateUserData<Container> {
   public:
    ~Container() override;
    // Returns the request queue for |modality|.
    OverlayRequestQueueImpl* QueueForModality(OverlayModality modality);

   private:
    friend class web::WebStateUserData<Container>;
    WEB_STATE_USER_DATA_KEY_DECL();
    explicit Container(web::WebState* web_state);

    std::map<OverlayModality, std::unique_ptr<OverlayRequestQueueImpl>> queues_;
  };

  // Adds and removes observers.
  void AddObserver(OverlayRequestQueueImplObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(OverlayRequestQueueImplObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Adds |request| to the queue.
  void AddRequest(std::unique_ptr<OverlayRequest> request);

  // Removes the front-most request from the queue and returns it.  Must be
  // called on a non-empty queue.
  void PopRequest();

  // OverlayRequestQueue:
  OverlayRequest* front_request() const override;

 private:
  // Private constructor called by container.
  OverlayRequestQueueImpl();

  base::ObserverList<OverlayRequestQueueImplObserver>::Unchecked observers_;
  // The queue used to hold the received requests.  Stored as a circular dequeue
  // to allow performant pop events from the front of the queue.
  base::circular_deque<std::unique_ptr<OverlayRequest>> requests_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_
