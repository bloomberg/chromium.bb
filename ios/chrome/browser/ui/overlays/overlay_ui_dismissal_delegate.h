// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_UI_DISMISSAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_UI_DISMISSAL_DELEGATE_H_

class OverlayRequest;

// Delegate class used to communicate dismissal events back to OverlayPresenter.
class OverlayUIDismissalDelegate {
 public:
  OverlayUIDismissalDelegate() = default;
  virtual ~OverlayUIDismissalDelegate() = default;

  // Called to notify the delegate that the UI for |request|'s UI is finished
  // being dismissed.
  virtual void OverlayUIDidFinishDismissal(OverlayRequest* request) = 0;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_UI_DISMISSAL_DELEGATE_H_
