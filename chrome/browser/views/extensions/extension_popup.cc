// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_popup.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               views::Widget* frame,
                               const gfx::Rect& relative_to)
    : BrowserBubble(host->view(),
      frame,
      gfx::Point()),
      relative_to_(relative_to),
      extension_host_(host) {
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 Source<Profile>(host->profile()));
}

ExtensionPopup::~ExtensionPopup() {
}

void ExtensionPopup::Show() {
  ResizeToView();

  // Anchor on the lower right corner and extend to the left.
  SetBounds(relative_to_.right() - width(), relative_to_.bottom(),
            width(), height());
  BrowserBubble::Show(true);
}

void ExtensionPopup::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_HOST_DID_STOP_LOADING) {
    // Once we receive did stop loading, the content will be complete and
    // the width will have been computed.  Now it's safe to show.
    if (extension_host_.get() == Details<ExtensionHost>(details).ptr())
      Show();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

// static
ExtensionPopup* ExtensionPopup::Show(const GURL& url, Browser* browser,
                                     const gfx::Rect& relative_to,
                                     int height) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeHandle())->GetWidget();
  ExtensionPopup* popup = new ExtensionPopup(host, frame, relative_to);
  gfx::Size sz = host->view()->GetPreferredSize();
  sz.set_height(height);
  host->view()->SetPreferredSize(sz);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show();

  return popup;
}
