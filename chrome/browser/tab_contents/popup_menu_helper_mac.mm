// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#include "chrome/browser/tab_contents/popup_menu_helper_mac.h"

#include "base/message_loop.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#import "chrome/browser/ui/cocoa/base_view.h"
#include "content/browser/renderer_host/render_view_host.h"
#import "content/common/chrome_application_mac.h"
#include "content/common/notification_source.h"
#include "webkit/glue/webmenurunner_mac.h"

PopupMenuHelper::PopupMenuHelper(RenderViewHost* render_view_host)
    : render_view_host_(render_view_host) {
  notification_registrar_.Add(
      this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(render_view_host));
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned) {
  // Retain the Cocoa view for the duration of the pop-up so that it can't be
  // dealloced if my Destroy() method is called while the pop-up's up (which
  // would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  RenderWidgetHostViewMac* rwhvm =
      static_cast<RenderWidgetHostViewMac*>(render_view_host_->view());
  scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view
      ([rwhvm->native_view() retain]);

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items
                                                fontSize:item_font_size
                                            rightAligned:right_aligned]);

  {
    // Make sure events can be pumped while the menu is up.
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    chrome_application_mac::ScopedSendingEvent sendingEventScoper;

    // Now run a SYNCHRONOUS NESTED EVENT LOOP until the pop-up is finished.
    [menu_runner runMenuInView:cocoa_view
                    withBounds:[cocoa_view flipRectToNSRect:bounds]
                  initialIndex:selected_item];
  }

  if (!render_view_host_) {
    // Bad news, the RenderViewHost got deleted while we were off running the
    // menu. Nothing to do.
    return;
  }

  if ([menu_runner menuItemWasChosen]) {
    render_view_host_->DidSelectPopupMenuItem(
        [menu_runner indexOfSelectedItem]);
  } else {
    render_view_host_->DidCancelPopupMenu();
  }
}

void PopupMenuHelper::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDER_WIDGET_HOST_DESTROYED);
  RenderViewHost* rvh = Source<RenderViewHost>(source).ptr();
  DCHECK_EQ(render_view_host_, rvh);
  render_view_host_ = NULL;
}

