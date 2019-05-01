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

// Enum type categorizing the different reasons overlay UI may be dismissed.
// Used in OverlayDismissalCallbacks to notify the OverlayManager why the
// overlay was dismissed.
enum class OverlayDismissalReason {
  // Used when the overlay UI is dismissed by the user.
  kUserInteraction,
  // Used when the overlay is hidden for HideOverlayUIForWebState().
  kHiding,
  // Used when the overlay is cancelled for CancelOverlayUIForWebState().
  kCancellation,
};

// Overlays presented by the UI delegate are provided with an
// OverlayDismissalCallback that is used to notify the OverlayManager when
// requested overlay UI has finished being dismissed.  |reason| is used to
// communicate what triggered the dismissal.  Overlays that are hidden may be
// shown again, so the callback will not update the OverlayRequestQueue.
// Overlays dismissed for user interaction or cancellation will never be shown
// again; executing the dismissal callback for these reasons will execute the
// request's callback and remove it from its queue.
typedef base::OnceCallback<void(OverlayDismissalReason reason)>
    OverlayDismissalCallback;

// Delegate that handles presenting the overlay UI for requests supplied to
// OverlayManager.
class OverlayUIDelegate {
 public:
  virtual ~OverlayUIDelegate() = default;

  // Called to show the overlay UI for the frontmost OverlayRequest in
  // |web_state|'s OverlayRequestQueue at |modality|.  |dismissal_callback| must
  // be stored by the delegate and called whenever the UI is finished being
  // dismissed for user interaction, hiding, or cancellation.
  virtual void ShowOverlayUIForWebState(
      web::WebState* web_state,
      OverlayModality modality,
      OverlayDismissalCallback dismissal_callback) = 0;

  // Called to hide the overlay UI for the frontmost OverlayRequest in
  // |web_state|'s OverlayRequestQueue at |modality|.  Hidden overlays may be
  // shown again, so they should be kept in memory or serialized so that the
  // state can be restored if shown again.  When hiding an overlay, the
  // presented UI must be dismissed, and the overlay's dismissal callback must
  // must be executed upon the dismissal's completion.
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
