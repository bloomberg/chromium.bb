// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_popup_host.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_popup_api.h"
#endif
#include "chrome/browser/profile.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_popup.h"
#endif
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"


ExtensionPopupHost* ExtensionPopupHost::PopupDelegate::popup_host() {
  if (!popup_host_.get())
    popup_host_.reset(new ExtensionPopupHost(this));

  return popup_host_.get();
}

ExtensionPopupHost::ExtensionPopupHost(PopupDelegate* delegate)
    :  // NO LINT
#if defined(TOOLKIT_VIEWS)
      child_popup_(NULL),
#endif
      delegate_(delegate) {
  DCHECK(delegate_);

  // Listen for view close requests, so that we can dismiss a hosted pop-up
  // view, if necessary.
  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(delegate_->GetBrowser()->profile()));
}

ExtensionPopupHost::~ExtensionPopupHost() {
  DismissPopup();
}

#if defined(TOOLKIT_VIEWS)
void ExtensionPopupHost::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
  DismissPopup();
}

void ExtensionPopupHost::BubbleBrowserWindowClosing(BrowserBubble* bubble) {
  DismissPopup();
}

void ExtensionPopupHost::BubbleGotFocus(BrowserBubble* bubble) {
}

void ExtensionPopupHost::BubbleLostFocus(BrowserBubble* bubble) {
  // TODO(twiz):  Dismiss the pop-up upon loss of focus of the bubble, but not
  // if the focus is transitioning to the host which owns the popup!
  // DismissPopup();
}
#endif  // defined(TOOLKIT_VIEWS)

void ExtensionPopupHost::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE) {
#if defined(TOOLKIT_VIEWS)
    // If we aren't the host of the popup, then disregard the notification.
    if (!child_popup_ ||
        Details<ExtensionHost>(child_popup_->host()) != details) {
      return;
    }
    DismissPopup();
#endif
  } else {
    NOTREACHED();
  }
}

void ExtensionPopupHost::DismissPopup() {
#if defined(TOOLKIT_VIEWS)
  if (child_popup_) {
    child_popup_->Hide();
    child_popup_->DetachFromBrowser();
    delete child_popup_;
    child_popup_ = NULL;

    PopupEventRouter::OnPopupClosed(
        delegate_->GetBrowser()->profile(),
        delegate_->GetRenderViewHost()->routing_id());
  }
#endif  // defined(TOOLKIT_VIEWS)
}
