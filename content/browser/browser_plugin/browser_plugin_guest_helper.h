// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_HELPER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_HELPER_H_

#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

class WebCursor;
struct ViewHostMsg_UpdateRect_Params;

namespace gfx {
class Size;
}

namespace content {
class BrowserPluginGuest;
class RenderViewHost;

// Helper for browser plugin guest.
//
// It overrides different WebContents messages that require special treatment
// for a WebContents to act as a guest. All functionality is handled by its
// delegate. This class exists so we have separation of messages requiring
// special handling, which can be moved to a message filter (IPC thread) for
// future optimization.
//
// The lifetime of this class is managed by the associated RenderViewHost. A
// BrowserPluginGuestHelper is created whenever a BrowserPluginGuest is created.
class BrowserPluginGuestHelper : public RenderViewHostObserver {
 public:
  BrowserPluginGuestHelper(BrowserPluginGuest* guest,
                           RenderViewHost* render_view_host);
  virtual ~BrowserPluginGuestHelper();

 protected:
  // RenderViewHostObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Message handlers
  void OnUpdateDragCursor(WebKit::WebDragOperation current_op);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);
  void OnHandleInputEventAck(WebKit::WebInputEvent::Type event_type,
                             InputEventAckState ack_result);
  void OnTakeFocus(bool reverse);
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnMsgHasTouchEventHandlers(bool has_handlers);
  void OnSetCursor(const WebCursor& cursor);

  BrowserPluginGuest* guest_;
  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_HELPER_H_
