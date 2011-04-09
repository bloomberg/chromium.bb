// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"

ExtensionPopupGtk* ExtensionPopupGtk::current_extension_popup_ = NULL;

// The minimum/maximum dimensions of the extension popup.
// The minimum is just a little larger than the size of a browser action button.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopupGtk::kMinWidth = 25;
const int ExtensionPopupGtk::kMinHeight = 25;
const int ExtensionPopupGtk::kMaxWidth = 800;
const int ExtensionPopupGtk::kMaxHeight = 600;

ExtensionPopupGtk::ExtensionPopupGtk(Browser* browser,
                                     ExtensionHost* host,
                                     GtkWidget* anchor,
                                     bool inspect)
    : browser_(browser),
      bubble_(NULL),
      host_(host),
      anchor_(anchor),
      being_inspected_(inspect),
      method_factory_(this) {
  host_->view()->SetContainer(this);

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
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
      if (Details<ExtensionHost>(host_.get()) == details)
        ShowPopup();
      break;
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      if (Details<ExtensionHost>(host_.get()) == details)
        DestroyPopup();
      break;
    case NotificationType::DEVTOOLS_WINDOW_CLOSING:
      // Make sure its the devtools window that inspecting our popup.
      if (Details<RenderViewHost>(host_->render_view_host()) != details)
        break;

      // If the devtools window is closing, we post a task to ourselves to
      // close the popup. This gives the devtools window a chance to finish
      // detaching from the inspected RenderViewHost.
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(&ExtensionPopupGtk::DestroyPopup));
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionPopupGtk::ShowPopup() {
  if (bubble_) {
    NOTREACHED();
    return;
  }

  if (being_inspected_) {
    DevToolsManager::GetInstance()->OpenDevToolsWindow(
        host_->render_view_host());
    // Listen for the the devtools window closing.
    registrar_.Add(this, NotificationType::DEVTOOLS_WINDOW_CLOSING,
        Source<Profile>(host_->profile()));
  }

  // Only one instance should be showing at a time. Get rid of the old one, if
  // any. Typically, |current_extension_popup_| will be NULL, but it can be
  // non-NULL if a browser action button is clicked while another extension
  // popup's extension host is still loading.
  if (current_extension_popup_)
    current_extension_popup_->DestroyPopup();
  current_extension_popup_ = this;

  // We'll be in the upper-right corner of the window for LTR languages, so we
  // want to put the arrow at the upper-right corner of the bubble to match the
  // page and app menus.
  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      !base::i18n::IsRTL() ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT;
  bubble_ = InfoBubbleGtk::Show(anchor_,
                                NULL,
                                host_->view()->native_view(),
                                arrow_location,
                                false,  // match_system_theme
                                !being_inspected_,  // grab_input
                                GtkThemeService::GetFrom(browser_->profile()),
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

void ExtensionPopupGtk::OnExtensionPreferredSizeChanged(
    ExtensionViewGtk* view,
    const gfx::Size& new_size) {
  int width = std::max(kMinWidth, std::min(kMaxWidth, new_size.width()));
  int height = std::max(kMinHeight, std::min(kMaxHeight, new_size.height()));

  view->render_view_host()->view()->SetSize(gfx::Size(width, height));
  gtk_widget_set_size_request(view->native_view(), width, height);
}

// static
void ExtensionPopupGtk::Show(const GURL& url, Browser* browser,
    GtkWidget* anchor, bool inspect) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  // This object will delete itself when the info bubble is closed.
  new ExtensionPopupGtk(browser, host, anchor, inspect);
}

gfx::Rect ExtensionPopupGtk::GetViewBounds() {
  return gfx::Rect(host_->view()->native_view()->allocation);
}
