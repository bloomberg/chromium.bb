// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_UI_DELEGATE_H_

#include <map>

#include "ios/chrome/browser/overlays/public/overlay_ui_delegate.h"

// Fake implementation of OverlayUIDelegate used for testing.
class FakeOverlayUIDelegate : public OverlayUIDelegate {
 public:
  FakeOverlayUIDelegate();
  ~FakeOverlayUIDelegate() override;

  // Enum describing the state of the overlay UI.
  enum class PresentationState {
    // Default state.  No overlays have been presented.
    kNotPresented,
    // An overlay is currently being presented.
    kPresented,
    // A presented overlay was dismissed by user interaction.
    kUserDismissed,
    // A presented overlay was hidden.
    kHidden,
    // A presented overlay was cancelled.
    kCancelled,
  };
  // Returns the presentation state for the overlay UI.
  PresentationState GetPresentationState(web::WebState* web_state);

  // Simulates the dismissal of overlay UI for |reason|.
  void SimulateDismissalForWebState(web::WebState* web_state,
                                    OverlayDismissalReason reason);

  // OverlayUIDelegate:
  void ShowOverlayUIForWebState(web::WebState* web_state,
                                OverlayModality modality,
                                OverlayDismissalCallback callback) override;
  void HideOverlayUIForWebState(web::WebState* web_state,
                                OverlayModality modality) override;
  void CancelOverlayUIForWebState(web::WebState* web_state,
                                  OverlayModality modality) override;

 private:
  // The presentation states for each WebState.
  std::map<web::WebState*, PresentationState> presentation_states_;
  // The callbacks for each WebState.
  std::map<web::WebState*, OverlayDismissalCallback> overlay_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_UI_DELEGATE_H_
