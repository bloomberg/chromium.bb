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
#if defined(OS_MACOSX)
struct ViewHostMsg_ShowPopup_Params;
#endif
struct ViewHostMsg_UpdateRect_Params;

namespace gfx {
class Size;
}

namespace content {
class BrowserPluginGuest;
class RenderViewHost;

// Helper for BrowserPluginGuest.
//
// The purpose of this class is to intercept messages from the guest RenderView
// before they are handled by the standard message handlers in the browser
// process. This permits overriding standard behavior with BrowserPlugin-
// specific behavior.
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
  // Returns whether a message should be forward to the helper's associated
  // BrowserPluginGuest.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

  BrowserPluginGuest* guest_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuestHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_HELPER_H_
