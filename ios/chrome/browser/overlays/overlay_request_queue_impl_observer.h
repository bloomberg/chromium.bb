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
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_OBSERVER_H_
