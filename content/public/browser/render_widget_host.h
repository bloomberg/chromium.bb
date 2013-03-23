// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/keyboard_listener.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

#if defined(TOOLKIT_GTK)
#include "ui/base/x/x11_util.h"
#elif defined(OS_MACOSX)
#include "skia/ext/platform_device.h"
#endif

class SkBitmap;

namespace gfx {
class Rect;
}

namespace WebKit {
struct WebScreenInfo;
}

namespace content {

class RenderProcessHost;
class RenderWidgetHostImpl;
class RenderWidgetHostView;

// A RenderWidgetHost manages the browser side of a browser<->renderer
// HWND connection.  The HWND lives in the browser process, and
// windows events are sent over IPC to the corresponding object in the
// renderer.  The renderer paints into shared memory, which we
// transfer to a backing store and blit to the screen when Windows
// sends us a WM_PAINT message.
//
// How Shutdown Works
//
// There are two situations in which this object, a RenderWidgetHost, can be
// instantiated:
//
// 1. By a WebContents as the communication conduit for a rendered web page.
//    The WebContents instantiates a derived class: RenderViewHost.
// 2. By a WebContents as the communication conduit for a select widget. The
//    WebContents instantiates the RenderWidgetHost directly.
//
// For every WebContents there are several objects in play that need to be
// properly destroyed or cleaned up when certain events occur.
//
// - WebContents - the WebContents itself, and its associated HWND.
// - RenderViewHost - representing the communication conduit with the child
//   process.
// - RenderWidgetHostView - the view of the web page content, message handler,
//   and plugin root.
//
// Normally, the WebContents contains a child RenderWidgetHostView that renders
// the contents of the loaded page. It has a WS_CLIPCHILDREN style so that it
// does no painting of its own.
//
// The lifetime of the RenderWidgetHostView is tied to the render process. If
// the render process dies, the RenderWidgetHostView goes away and all
// references to it must become NULL.
//
// RenderViewHost (a RenderWidgetHost subclass) is the conduit used to
// communicate with the RenderView and is owned by the WebContents. If the
// render process crashes, the RenderViewHost remains and restarts the render
// process if needed to continue navigation.
//
// Some examples of how shutdown works:
//
// For a WebContents, its Destroy method tells the RenderViewHost to
// shut down the render process and die.
//
// When the render process is destroyed it destroys the View: the
// RenderWidgetHostView, which destroys its HWND and deletes that object.
//
// For select popups, the situation is a little different. The RenderWidgetHost
// associated with the select popup owns the view and itself (is responsible
// for destroying itself when the view is closed). The WebContents's only
// responsibility is to select popups is to create them when it is told to. When
// the View is destroyed via an IPC message (for when WebCore destroys the
// popup, e.g. if the user selects one of the options), or because
// WM_CANCELMODE is received by the view, the View schedules the destruction of
// the render process. However in this case since there's no WebContents
// container, when the render process is destroyed, the RenderWidgetHost just
// deletes itself, which is safe because no one else should have any references
// to it (the WebContents does not).
//
// It should be noted that the RenderViewHost, not the RenderWidgetHost,
// handles IPC messages relating to the render process going away, since the
// way a RenderViewHost (WebContents) handles the process dying is different to
// the way a select popup does. As such the RenderWidgetHostView handles these
// messages for select popups. This placement is more out of convenience than
// anything else. When the view is live, these messages are forwarded to it by
// the RenderWidgetHost's IPC message map.
class CONTENT_EXPORT RenderWidgetHost : public IPC::Sender {
 public:
  // Free all backing stores used for rendering to drop memory usage.
  static void RemoveAllBackingStores();

  // Returns the size of all the backing stores used for rendering
  static size_t BackingStoreMemorySize();

  virtual ~RenderWidgetHost() {}

  // Edit operations.
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void CopyToFindPboard() = 0;
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void Delete() = 0;
  virtual void SelectAll() = 0;

  // Update the text direction of the focused input element and notify it to a
  // renderer process.
  // These functions have two usage scenarios: changing the text direction
  // from a menu (as Safari does), and; changing the text direction when a user
  // presses a set of keys (as IE and Firefox do).
  // 1. Change the text direction from a menu.
  // In this scenario, we receive a menu event only once and we should update
  // the text direction immediately when a user chooses a menu item. So, we
  // should call both functions at once as listed in the following snippet.
  //   void RenderViewHost::SetTextDirection(WebTextDirection direction) {
  //     UpdateTextDirection(direction);
  //     NotifyTextDirection();
  //   }
  // 2. Change the text direction when pressing a set of keys.
  // Because of auto-repeat, we may receive the same key-press event many
  // times while we presses the keys and it is nonsense to send the same IPC
  // message every time when we receive a key-press event.
  // To suppress the number of IPC messages, we just update the text direction
  // when receiving a key-press event and send an IPC message when we release
  // the keys as listed in the following snippet.
  //   if (key_event.type == WebKeyboardEvent::KEY_DOWN) {
  //     if (key_event.windows_key_code == 'A' &&
  //         key_event.modifiers == WebKeyboardEvent::CTRL_KEY) {
  //       UpdateTextDirection(dir);
  //     } else {
  //       CancelUpdateTextDirection();
  //     }
  //   } else if (key_event.type == WebKeyboardEvent::KEY_UP) {
  //     NotifyTextDirection();
  //   }
  // Once we cancel updating the text direction, we have to ignore all
  // succeeding UpdateTextDirection() requests until calling
  // NotifyTextDirection(). (We may receive keydown events even after we
  // canceled updating the text direction because of auto-repeat.)
  // Note: we cannot undo this change for compatibility with Firefox and IE.
  virtual void UpdateTextDirection(WebKit::WebTextDirection direction) = 0;
  virtual void NotifyTextDirection() = 0;

  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Sets whether the renderer should show controls in an active state.  On all
  // platforms except mac, that's the same as focused. On mac, the frontmost
  // window will show active controls even if the focus is not in the web
  // contents, but e.g. in the omnibox.
  virtual void SetActive(bool active) = 0;

  // Copies the given subset of the backing store, and passes the result as a
  // bitmap to a callback.
  //
  // If |src_rect| is empty, the whole contents is copied. If non empty
  // |accelerated_dst_size| is given and accelerated compositing is active, the
  // content is shrunk so that it fits in |accelerated_dst_size|. If
  // |accelerated_dst_size| is larger than the content size, the content is not
  // resized. If |accelerated_dst_size| is empty, the size copied from the
  // source contents is used. |callback| is invoked with true on success, false
  // otherwise, along with a SkBitmap containing the copied pixel data.
  //
  // NOTE: |callback| is called synchronously if the backing store is available.
  // When accelerated compositing is active, |callback| may be called
  // asynchronously.
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) = 0;
#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  virtual bool CopyFromBackingStoreToGtkWindow(const gfx::Rect& dest_rect,
                                               GdkWindow* target) = 0;
#elif defined(OS_MACOSX)
  virtual gfx::Size GetBackingStoreSize() = 0;
  virtual bool CopyFromBackingStoreToCGContext(const CGRect& dest_rect,
                                               CGContextRef target) = 0;
#endif

  // Send a command to the renderer to turn on full accessibility.
  virtual void EnableFullAccessibilityMode() = 0;

  // Forwards the given message to the renderer. These are called by
  // the view when it has received a message.
  virtual void ForwardMouseEvent(
      const WebKit::WebMouseEvent& mouse_event) = 0;
  virtual void ForwardWheelEvent(
      const WebKit::WebMouseWheelEvent& wheel_event) = 0;
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) = 0;

  virtual const gfx::Vector2d& GetLastScrollOffset() const = 0;

  virtual RenderProcessHost* GetProcess() const = 0;

  virtual int GetRoutingID() const = 0;

  // Gets the View of this RenderWidgetHost. Can be NULL, e.g. if the
  // RenderWidget is being destroyed or the render process crashed. You should
  // never cache this pointer since it can become NULL if the renderer crashes,
  // instead you should always ask for it using the accessor.
  virtual RenderWidgetHostView* GetView() const = 0;

  // Returns true if the renderer is loading, false if not.
  virtual bool IsLoading() const = 0;

  // Returns true if this is a RenderViewHost, false if not.
  virtual bool IsRenderView() const = 0;

  // This tells the renderer to paint into a bitmap and return it,
  // regardless of whether the tab is hidden or not.  It resizes the
  // web widget to match the |page_size| and then returns the bitmap
  // scaled so it matches the |desired_size|, so that the scaling
  // happens on the rendering thread.  When the bitmap is ready, the
  // renderer sends a PaintAtSizeACK to this host, and a
  // RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK notification is issued.
  // Note that this bypasses most of the update logic that is normally invoked,
  // and doesn't put the results into the backing store.
  virtual void PaintAtSize(TransportDIB::Handle dib_handle,
                           int tag,
                           const gfx::Size& page_size,
                           const gfx::Size& desired_size) = 0;

  // Makes an IPC call to tell webkit to replace the currently selected word
  // or a word around the cursor.
  virtual void Replace(const string16& word) = 0;

  // Makes an IPC call to tell webkit to replace the misspelling in the current
  // selection.
  virtual void ReplaceMisspelling(const string16& word) = 0;

  // Called to notify the RenderWidget that the resize rect has changed without
  // the size of the RenderWidget itself changing.
  virtual void ResizeRectChanged(const gfx::Rect& new_rect) = 0;

  // Restart the active hang monitor timeout. Clears all existing timeouts and
  // starts with a new one.  This can be because the renderer has become
  // active, the tab is being hidden, or the user has chosen to wait some more
  // to give the tab a chance to become active and we don't want to display a
  // warning too soon.
  virtual void RestartHangMonitorTimeout() = 0;

  virtual void SetIgnoreInputEvents(bool ignore_input_events) = 0;

  // Stops loading the page.
  virtual void Stop() = 0;

  // Called to notify the RenderWidget that it has been resized.
  virtual void WasResized() = 0;

  // Access to the implementation's IPC::Listener::OnMessageReceived. Intended
  // only for test code.

  // Add a keyboard listener that can handle key presses without requiring
  // focus.
  virtual void AddKeyboardListener(KeyboardListener* listener) = 0;

  // Remove a keyboard listener.
  virtual void RemoveKeyboardListener(KeyboardListener* listener) = 0;

  // Get the screen info corresponding to this render widget.
  virtual void GetWebScreenInfo(WebKit::WebScreenInfo* result) = 0;

  // Grabs snapshot from renderer side and returns the bitmap to a callback.
  // If |src_rect| is empty, the whole contents is copied. This is an expensive
  // operation due to the IPC, but it can be used as a fallback method when
  // CopyFromBackingStore fails due to the backing store not being available or,
  // in composited mode, when the accelerated surface is not available to the
  // browser side.
  virtual void GetSnapshotFromRenderer(
      const gfx::Rect& src_subrect,
      const base::Callback<void(bool, const SkBitmap&)>& callback) = 0;

 protected:
  friend class RenderWidgetHostImpl;

  // Retrieves the implementation class.  Intended only for code
  // within content/.  This method is necessary because
  // RenderWidgetHost is the root of a diamond inheritance pattern, so
  // subclasses inherit it virtually, which removes our ability to
  // static_cast to the subclass.
  virtual RenderWidgetHostImpl* AsRenderWidgetHostImpl() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_
