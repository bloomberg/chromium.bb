// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_popup_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

ExtensionPopupGtk* ExtensionPopupGtk::current_extension_popup_ = NULL;

ExtensionPopupGtk::ExtensionPopupGtk(Browser* browser,
                                     ExtensionHost* host,
                                     const gfx::Rect& relative_to)
    : browser_(browser),
      bubble_(NULL),
      host_(host),
      relative_to_(relative_to) {

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading()) {
    ShowPopup();
  } else {
    registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                   Source<Profile>(host->profile()));
  }

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(host->profile()));
}

ExtensionPopupGtk::~ExtensionPopupGtk() {
}

void ExtensionPopupGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (Details<ExtensionHost>(host_.get()) != details)
    return;

  if (type == NotificationType::EXTENSION_HOST_DID_STOP_LOADING) {
    ShowPopup();
  } else if (type == NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE) {
    DestroyPopup();
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionPopupGtk::ShowPopup() {
  if (bubble_) {
    NOTREACHED();
    return;
  }

  // Only one instance should be showing at a time.
  DCHECK(!current_extension_popup_);
  current_extension_popup_ = this;

  // We'll be in the upper-right corner of the window for LTR languages, so we
  // want to put the arrow at the upper-right corner of the bubble to match the
  // page and app menus.
  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT;
  bubble_ = InfoBubbleGtk::Show(browser_->window()->GetNativeHandle(),
                                relative_to_,
                                host_->view()->native_view(),
                                arrow_location,
                                false,
                                GtkThemeProvider::GetFrom(browser_->profile()),
                                this);
}

bool ExtensionPopupGtk::DestroyPopup() {
  if (!bubble_) {
    NOTREACHED();
    return false;
  }

  bubble_->Close();
  return true;
}

void ExtensionPopupGtk::InfoBubbleClosing(InfoBubbleGtk* bubble,
                                          bool closed_by_escape) {
  current_extension_popup_ = NULL;
  delete this;
}

// static
void ExtensionPopupGtk::Show(const GURL& url, Browser* browser,
                             const gfx::Rect& relative_to) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  // This object will delete itself when the info bubble is closed.
  new ExtensionPopupGtk(browser, host, relative_to);
}

gfx::Rect ExtensionPopupGtk::GetViewBounds() {
  return gfx::Rect(host_->view()->native_view()->allocation);
}
