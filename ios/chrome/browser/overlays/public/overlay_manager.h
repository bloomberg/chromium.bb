// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_

#include <memory>

#include "ios/chrome/browser/overlays/public/overlay_modality.h"

class Browser;
class OverlayManagerObserver;
class OverlayUIDelegate;
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

  // Sets the UI delegate for the manager.  Upon being set, the OverlayManager
  // will attempt to begin presenting overlay UI for the active WebState in its
  // Browser.
  virtual void SetUIDelegate(OverlayUIDelegate* ui_delegate) = 0;

  // Adds and removes observers.
  virtual void AddObserver(OverlayManagerObserver* observer) = 0;
  virtual void RemoveObserver(OverlayManagerObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_MANAGER_H_
