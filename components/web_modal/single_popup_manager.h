// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_SINGLE_POPUP_MANAGER_H_
#define COMPONENTS_WEB_MODAL_SINGLE_POPUP_MANAGER_H_

#include "components/web_modal/native_web_contents_modal_dialog.h"

namespace content {
class WebContents;
}

namespace web_modal {

// Interface from SinglePopupManager to PopupManager.
class SinglePopupManagerDelegate {
 public:
  SinglePopupManagerDelegate() {}
  virtual ~SinglePopupManagerDelegate() {}

  // Notify the delegate that the dialog is closing. The
  // calling SinglePopupManager will be deleted before the end of this call.
  virtual void WillClose(NativePopup popup) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SinglePopupManagerDelegate);
};

// Provides an interface for platform-specific UI implementation for popups.
// Each object will manage a single NativePopup window during its lifecycle.
// The rule of thumb is that only one popup will be shown at a time per tab.
// (Exceptions exist.)
//
// Implementation classes should accept a NativePopup at construction time
// and register to be notified when the dialog is closing, so that it can
// notify its delegate (WillClose method).
class SinglePopupManager {
 public:
  virtual ~SinglePopupManager() {}

  // If the manager returns non-null to this call, it is bound to a particular
  // tab and will be hidden and shown if that tab visibility changes.
  virtual content::WebContents* GetBoundWebContents() = 0;

  // Makes the popup visible.
  virtual void Show() = 0;

  // Hides the popup without closing it.
  virtual void Hide() = 0;

  // Closes the popup.
  // This method must call WillClose() on the delegate.
  // Important note: this object will be deleted at the close of that
  // invocation.
  virtual void Close() = 0;

  // Sets focus on the popup.
  virtual void Focus() = 0;

  // Pulsates the popup to draw the user's attention to it.
  virtual void Pulse() = 0;

  // Returns the popup under management by this object.
  virtual NativePopup popup() = 0;

  // Returns true if the popup under management was initiated by a user
  // gesture. When this is true, the popup manager will hide any existing
  // popups and move the newly-created NativePopup to the top of the display
  // queue.
  virtual bool HasUserGesture() = 0;

  // Returns true if the popup under management is tolerant of being overlapped
  // by other popups. This may be true for large web-modals which cover most of
  // the window real estate (e.g. print preview).
  virtual bool MayBeOverlapped() = 0;

  // Returns true if the popup under management should close if there is a
  // navigation underneath it, including the showing of any interstitial
  // content. Popups which relate to the web content being displayed will
  // ordinarily set this to true.
  // TODO(gbillock): should be generalized in code location or by splitting
  // the API to support the same-origin logic in WCMDM::DidNavigateMainFrame.
  // TBD right now because we're not sure which knobs need to be set here.
  virtual bool ShouldCloseOnNavigation() = 0;

 protected:
  SinglePopupManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SinglePopupManager);
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_SINGLE_POPUP_MANAGER_H_
