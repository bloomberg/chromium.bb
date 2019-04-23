// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_OBSERVER_H_

class OverlayManager;
class OverlayRequest;
namespace web {
class WebState;
}

// Observer interface for objects interested in overlay UI presentation by
// OverlayManager.
class OverlayManagerObserver {
 public:
  OverlayManagerObserver() = default;
  virtual ~OverlayManagerObserver() = default;

  // Called when |manager| is about to show the overlay UI for |request|
  // alongside |web_state|'s content area.
  virtual void WillShowOverlay(OverlayManager* manager,
                               OverlayRequest* request,
                               web::WebState* web_state) {}

  // Called when |manager| is finished dismissing the overlay UI for |request|
  // that was presented alongside |web_state|'s content area.  After this
  // function is called, the |request| will be removed from its queue and
  // deleted.
  virtual void DidHideOverlay(OverlayManager* manager,
                              OverlayRequest* request,
                              web::WebState* web_state) {}

  // Called when |manager| is destroyed.
  virtual void OverlayManagerDestroyed(OverlayManager* manager) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_OBSERVER_H_
