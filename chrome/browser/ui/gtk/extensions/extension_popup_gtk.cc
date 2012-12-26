// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "googleurl/src/gurl.h"

using content::RenderViewHost;

ExtensionPopupGtk* ExtensionPopupGtk::current_extension_popup_ = NULL;

// The minimum/maximum dimensions of the extension popup.
// The minimum is just a little larger than the size of a browser action button.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopupGtk::kMinWidth = 25;
const int ExtensionPopupGtk::kMinHeight = 25;
const int ExtensionPopupGtk::kMaxWidth = 800;
const int ExtensionPopupGtk::kMaxHeight = 600;

ExtensionPopupGtk::ExtensionPopupGtk(Browser* browser,
                                     extensions::ExtensionHost* host,
                                     GtkWidget* anchor,
                                     ShowAction show_action)
    : browser_(browser),
      bubble_(NULL),
      host_(host),
      anchor_(anchor),
      weak_factory_(this) {
  host_->view()->SetContainer(this);
  being_inspected_ = show_action == SHOW_AND_INSPECT;

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading()) {
    ShowPopup();
  } else {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                   content::Source<Profile>(host->profile()));
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));

  registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED,
                 content::Source<Profile>(host->profile()));

  if (!being_inspected_) {
    registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED,
                   content::Source<Profile>(host->profile()));
  }
}

ExtensionPopupGtk::~ExtensionPopupGtk() {
}

// static
void ExtensionPopupGtk::Show(const GURL& url, Browser* browser,
    GtkWidget* anchor, ShowAction show_action) {
  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(browser->profile())->process_manager();
  DCHECK(manager);
  if (!manager)
    return;

  extensions::ExtensionHost* host = manager->CreatePopupHost(url, browser);
  // This object will delete itself when the bubble is closed.
  new ExtensionPopupGtk(browser, host, anchor, show_action);
}

void ExtensionPopupGtk::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
      if (content::Details<extensions::ExtensionHost>(host_.get()) == details)
        ShowPopup();
      break;
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      if (content::Details<extensions::ExtensionHost>(host_.get()) == details)
        DestroyPopup();
      break;
    case content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED:
      // Make sure it's the devtools window that is inspecting our popup.
      if (content::Details<RenderViewHost>(host_->render_view_host()) !=
          details)
        break;

      // Make sure that the popup won't go away when the inspector is activated.
      if (bubble_)
        bubble_->StopGrabbingInput();

      being_inspected_ = true;
      break;
    case content::NOTIFICATION_DEVTOOLS_AGENT_DETACHED:
      // Make sure it's the devtools window that is inspecting our popup.
      if (content::Details<RenderViewHost>(host_->render_view_host()) !=
          details)
        break;

      // If the devtools window is closing, we post a task to ourselves to
      // close the popup. This gives the devtools window a chance to finish
      // detaching from the inspected RenderViewHost.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionPopupGtk::DestroyPopupWithoutResult,
                     weak_factory_.GetWeakPtr()));
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

void ExtensionPopupGtk::BubbleClosing(BubbleGtk* bubble,
                                      bool closed_by_escape) {
  current_extension_popup_ = NULL;
  delete this;
}

void ExtensionPopupGtk::OnExtensionSizeChanged(
    ExtensionViewGtk* view,
    const gfx::Size& new_size) {
  int width = std::max(kMinWidth, std::min(kMaxWidth, new_size.width()));
  int height = std::max(kMinHeight, std::min(kMaxHeight, new_size.height()));

  view->render_view_host()->GetView()->SetSize(gfx::Size(width, height));
  gtk_widget_set_size_request(view->native_view(), width, height);
}

bool ExtensionPopupGtk::DestroyPopup() {
  if (!bubble_) {
    NOTREACHED();
    return false;
  }

  bubble_->Close();
  return true;
}

void ExtensionPopupGtk::ShowPopup() {
  if (bubble_) {
    NOTREACHED();
    return;
  }

  if (being_inspected_)
    DevToolsWindow::OpenDevToolsWindow(host_->render_view_host());

  // Only one instance should be showing at a time. Get rid of the old one, if
  // any. Typically, |current_extension_popup_| will be NULL, but it can be
  // non-NULL if a browser action button is clicked while another extension
  // popup's extension host is still loading.
  if (current_extension_popup_)
    current_extension_popup_->DestroyPopup();
  current_extension_popup_ = this;

  GtkWidget* border_box = gtk_alignment_new(0, 0, 1.0, 1.0);
  // This border is necessary so the bubble's corners do not get cut off by the
  // render view.
  gtk_container_set_border_width(GTK_CONTAINER(border_box), 2);
  gtk_container_add(GTK_CONTAINER(border_box), host_->view()->native_view());

  // We'll be in the upper-right corner of the window for LTR languages, so we
  // want to put the arrow at the upper-right corner of the bubble to match the
  // page and app menus.
  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,
                            border_box,
                            BubbleGtk::ANCHOR_TOP_RIGHT,
                            being_inspected_ ? 0 :
                                BubbleGtk::POPUP_WINDOW | BubbleGtk::GRAB_INPUT,
                            GtkThemeService::GetFrom(browser_->profile()),
                            this);
}

void ExtensionPopupGtk::DestroyPopupWithoutResult() {
  DestroyPopup();
}

gfx::Rect ExtensionPopupGtk::GetViewBounds() {
  GtkAllocation allocation;
  gtk_widget_get_allocation(host_->view()->native_view(), &allocation);
  return gfx::Rect(allocation);
}
