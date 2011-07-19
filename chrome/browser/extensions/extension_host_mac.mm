// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host_mac.h"

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "content/common/native_web_keyboard_event.h"

ExtensionHostMac::~ExtensionHostMac() {
  // If there is a popup open for this host's extension, close it.
  ExtensionPopupController* popup = [ExtensionPopupController popup];
  if ([[popup window] isVisible] &&
      [popup extensionHost]->extension() == this->extension()) {
    InfoBubbleWindow* window = (InfoBubbleWindow*)[popup window];
    [window setDelayOnClose:NO];
    [popup close];
  }
}

RenderWidgetHostView* ExtensionHostMac::CreateNewWidgetInternal(
    int route_id,
    WebKit::WebPopupType popup_type) {
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosed.
  RenderWidgetHostView* widget_view =
      ExtensionHost::CreateNewWidgetInternal(route_id, popup_type);
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_view);
  [widget_view_mac->native_view() retain];

  return widget_view;
}

void ExtensionHostMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  ExtensionHost::ShowCreatedWidgetInternal(widget_host_view, initial_pos);

  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the reference we
  // took in CreateNewWidgetInternal().
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_host_view);
  [widget_view_mac->native_view() release];
}

void ExtensionHostMac::UnhandledKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char ||
      extension_host_type() != ViewType::EXTENSION_POPUP) {
    return;
  }

  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>([view()->native_view() window]);
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);
  [event_window redispatchKeyEvent:event.os_event];
}
