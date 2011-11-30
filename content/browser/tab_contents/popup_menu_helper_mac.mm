// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#include "content/browser/tab_contents/popup_menu_helper_mac.h"

#import "base/mac/scoped_sending_event.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#import "ui/base/cocoa/base_view.h"
#include "webkit/glue/webmenurunner_mac.h"

PopupMenuHelper::PopupMenuHelper(RenderViewHost* render_view_host)
    : render_view_host_(render_view_host) {
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      content::Source<RenderWidgetHost>(render_view_host));
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
    base::mac::ScopedSendingEvent sending_event_scoper;

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
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED);
  DCHECK(content::Source<RenderWidgetHost>(source).ptr() == render_view_host_);
  render_view_host_ = NULL;
}

