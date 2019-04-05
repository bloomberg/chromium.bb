// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_

class OverlayRequest;
class OverlayRequestQueue;

// Observer class for the request queue.
class OverlayRequestQueueImplObserver {
 public:
  // Called after |request| has been added to |queue|.
  virtual void OnRequestAdded(OverlayRequestQueue* queue,
                              OverlayRequest* request) {}

  // Called after |request| is removed from |queue|.  |request| is deleted
  // immediately after this callback.
  virtual void OnRequestRemoved(OverlayRequestQueue* queue,
                                OverlayRequest* request) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_
