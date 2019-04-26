// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_UI_DELEGATE_H_

#include "base/callback.h"

#include "ios/chrome/browser/overlays/public/overlay_modality.h"

namespace web {
class WebState;
}

// Overlays presented by the UI delegate are provided with an
// OverlayDismissalCallback that is used to notify the OverlayManager when
// requested overlay UI has finished being dismissed.
typedef base::OnceCallback<void(void)> OverlayDismissalCallback;

// Delegate that handles presenting the overlay UI for requests supplied to
// OverlayService.
class OverlayUIDelegate {
 public:
  virtual ~OverlayUIDelegate() = default;

  // Called to show the overlay UI for the frontmost OverlayRequest in
  // |web_state|'s OverlayRequestQueue at |modality|.  |dismissal_callback| must
  // be called  when the UI is finished being dismissed, either by cancellation
  // or by user interaction.
  virtual void ShowOverlayUIForWebState(
      web::WebState* web_state,
      OverlayModality modality,
      OverlayDismissalCallback dismissal_callback) = 0;

  // Called to hide the overlay UI for the frontmost OverlayRequest in
  // |web_state|'s OverlayRequestQueue at |modality|.  Hidden overlays may
  // be shown again, so they should be kept in memory or serialized so that the
  // state can be restored if shown again.  The dismissal callback should not
  // be executed upon hiding overlays.
  virtual void HideOverlayUIForWebState(web::WebState* web_state,
                                        OverlayModality modality) = 0;

  // Called to cancel the overlay UI for the frontmost OverlayRequest in
  // |web_state|'s OverlayRequestQueue at |modality|.  If the UI is presented,
  // it should be dismissed, and the dismissal callback should be executed upon
  // the dismissal's completion.  Otherwise, any state corresponding to any
  // hidden overlays should be cleaned up.
  virtual void CancelOverlayUIForWebState(web::WebState* web_state,
                                          OverlayModality modality) = 0;

 protected:
  OverlayUIDelegate() = default;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_UI_DELEGATE_H_
