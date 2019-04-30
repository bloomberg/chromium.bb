// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_

#include <stddef.h>

class OverlayRequest;
class OverlayRequestQueueImpl;

// Observer class for the request queue.
class OverlayRequestQueueImplObserver {
 public:
  // Called after |request| has been added to |queue|.
  virtual void OnRequestAdded(OverlayRequestQueueImpl* queue,
                              OverlayRequest* request) {}

  // Called after |request| is removed from |queue|, with |frontmost| indicating
  // whether the removed request was at the front of the queue.  |request| is
  // deleted immediately after this callback.
  virtual void OnRequestRemoved(OverlayRequestQueueImpl* queue,
                                OverlayRequest* request,
                                bool frontmost) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_
