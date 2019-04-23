// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_

#include <memory>

#include "ios/chrome/browser/overlays/public/overlay_modality.h"

class Browser;
class OverlayRequest;
class OverlayRequestQueue;
class OverlayManagerObserver;
namespace web {
class WebState;
}

// OverlayManager can be used to schedule the presentation of overlay UI that is
// presented alongside a specific WebState's content area.
class OverlayManager {
 public:
  virtual ~OverlayManager() = default;

  // Retrieves the OverlayManager for |browser| that manages overlays at
  // |modality|, creating one if necessary.
  static OverlayManager* FromBrowser(Browser* browser,
                                     OverlayModality modality);

  // Adds and removes observers.
  virtual void AddObserver(OverlayManagerObserver* observer) = 0;
  virtual void RemoveObserver(OverlayManagerObserver* observer) = 0;

  // Adds |request| to be displayed alongside |web_state|'s content area at the
  // manager's modality.  Both are expected to be non-null.
  virtual void AddRequest(std::unique_ptr<OverlayRequest> request,
                          web::WebState* web_state) = 0;

  // Returns the OverlayRequestQueue used by this manager to schedule overlays
  // alongside |web_state|'s content area.
  virtual OverlayRequestQueue* GetQueueForWebState(
      web::WebState* web_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_
