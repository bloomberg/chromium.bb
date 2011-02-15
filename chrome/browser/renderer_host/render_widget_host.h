// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/gtest_prod_util.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/property_bag.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace gfx {
class Rect;
}

namespace WebKit {
class WebInputEvent;
class WebMouseEvent;
struct WebCompositionUnderline;
struct WebScreenInfo;
}

class BackingStore;
class PaintObserver;
class RenderProcessHost;
class RenderWidgetHostView;
class TransportDIB;
class WebCursor;
struct ViewHostMsg_UpdateRect_Params;

// This class manages the browser side of a browser<->renderer HWND connection.
// The HWND lives in the browser process, and windows events are sent over
// IPC to the corresponding object in the renderer.  The renderer paints into
// shared memory, which we transfer to a backing store and blit to the screen
// when Windows sends us a WM_PAINT message.
//
// How Shutdown Works
//
// There are two situations in which this object, a RenderWidgetHost, can be
// instantiated:
//
// 1. By a TabContents as the communication conduit for a rendered web page.
//    The TabContents instantiates a derived class: RenderViewHost.
// 2. By a TabContents as the communication conduit for a select widget. The
//    TabContents instantiates the RenderWidgetHost directly.
//
// For every TabContents there are several objects in play that need to be
// properly destroyed or cleaned up when certain events occur.
//
// - TabContents - the TabContents itself, and its associated HWND.
// - RenderViewHost - representing the communication conduit with the child
//   process.
// - RenderWidgetHostView - the view of the web page content, message handler,
//   and plugin root.
//
// Normally, the TabContents contains a child RenderWidgetHostView that renders
// the contents of the loaded page. It has a WS_CLIPCHILDREN style so that it
// does no painting of its own.
//
// The lifetime of the RenderWidgetHostView is tied to the render process. If
// the render process dies, the RenderWidgetHostView goes away and all
// references to it must become NULL. If the TabContents finds itself without a
// RenderWidgetHostView, it paints Sad Tab instead.
//
// RenderViewHost (a RenderWidgetHost subclass) is the conduit used to
// communicate with the RenderView and is owned by the TabContents. If the
// render process crashes, the RenderViewHost remains and restarts the render
// process if needed to continue navigation.
//
// The TabContents is itself owned by the NavigationController in which it
// resides.
//
// Some examples of how shutdown works:
//
// When a tab is closed (either by the user, the web page calling window.close,
// etc) the TabStrip destroys the associated NavigationController, which calls
// Destroy on each TabContents it owns.
//
// For a TabContents, its Destroy method tells the RenderViewHost to
// shut down the render process and die.
//
// When the render process is destroyed it destroys the View: the
// RenderWidgetHostView, which destroys its HWND and deletes that object.
//
// For select popups, the situation is a little different. The RenderWidgetHost
// associated with the select popup owns the view and itself (is responsible
// for destroying itself when the view is closed). The TabContents's only
// responsibility is to select popups is to create them when it is told to. When
// the View is destroyed via an IPC message (for when WebCore destroys the
// popup, e.g. if the user selects one of the options), or because
// WM_CANCELMODE is received by the view, the View schedules the destruction of
// the render process. However in this case since there's no TabContents
// container, when the render process is destroyed, the RenderWidgetHost just
// deletes itself, which is safe because no one else should have any references
// to it (the TabContents does not).
//
// It should be noted that the RenderViewHost, not the RenderWidgetHost,
// handles IPC messages relating to the render process going away, since the
// way a RenderViewHost (TabContents) handles the process dying is different to
// the way a select popup does. As such the RenderWidgetHostView handles these
// messages for select popups. This placement is more out of convenience than
// anything else. When the view is live, these messages are forwarded to it by
// the RenderWidgetHost's IPC message map.
//
class RenderWidgetHost : public IPC::Channel::Listener,
                         public IPC::Channel::Sender {
 public:
  // Used as the details object for a
  // RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK notification.
  struct PaintAtSizeAckDetails {
    // The tag that was passed to the PaintAtSize() call that triggered this
    // ack.
    int tag;
    gfx::Size size;
  };

  // routing_id can be MSG_ROUTING_NONE, in which case the next available
  // routing id is taken from the RenderProcessHost.
  RenderWidgetHost(RenderProcessHost* process, int routing_id);
  virtual ~RenderWidgetHost();

  // Gets/Sets the View of this RenderWidgetHost. Can be NULL, e.g. if the
  // RenderWidget is being destroyed or the render process crashed. You should
  // never cache this pointer since it can become NULL if the renderer crashes,
  // instead you should always ask for it using the accessor.
  void set_view(RenderWidgetHostView* view) { view_ = view; }
  RenderWidgetHostView* view() const { return view_; }

  RenderProcessHost* process() const { return process_; }
  int routing_id() const { return routing_id_; }
  bool renderer_accessible() { return renderer_accessible_; }

  // Returns the property bag for this widget, where callers can add extra data
  // they may wish to associate with it. Returns a pointer rather than a
  // reference since the PropertyAccessors expect this.
  const PropertyBag* property_bag() const { return &property_bag_; }
  PropertyBag* property_bag() { return &property_bag_; }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  void Init();

  // Tells the renderer to die and then calls Destroy().
  virtual void Shutdown();

  // Manual RTTI FTW. We are not hosting a web page.
  virtual bool IsRenderView() const;

  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Sends a message to the corresponding object in the renderer.
  virtual bool Send(IPC::Message* msg);

  // Called to notify the RenderWidget that it has been hidden or restored from
  // having been hidden.
  void WasHidden();
  void WasRestored();

  // Called to notify the RenderWidget that it has been resized.
  void WasResized();

  // Called to notify the RenderWidget that its associated native window got
  // focused.
  virtual void GotFocus();

  // Tells the renderer it got/lost focus.
  void Focus();
  void Blur();
  virtual void LostCapture();

  // Tells us whether the page is rendered directly via the GPU process.
  bool is_accelerated_compositing_active() {
    return is_accelerated_compositing_active_;
  }

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // This tells the renderer to paint into a bitmap and return it,
  // regardless of whether the tab is hidden or not.  It resizes the
  // web widget to match the |page_size| and then returns the bitmap
  // scaled so it matches the |desired_size|, so that the scaling
  // happens on the rendering thread.  When the bitmap is ready, the
  // renderer sends a PaintAtSizeACK to this host, and a
  // RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK notification is issued.
  // Note that this bypasses most of the update logic that is normally invoked,
  // and doesn't put the results into the backing store.
  void PaintAtSize(TransportDIB::Handle dib_handle,
                   int tag,
                   const gfx::Size& page_size,
                   const gfx::Size& desired_size);

  // Get access to the widget's backing store.  If a resize is in progress,
  // then the current size of the backing store may be less than the size of
  // the widget's view.  If you pass |force_create| as true, then the backing
  // store will be created if it doesn't exist. Otherwise, NULL will be returned
  // if the backing store doesn't already exist. It will also return NULL if the
  // backing store could not be created.
  BackingStore* GetBackingStore(bool force_create);

  // Allocate a new backing store of the given size. Returns NULL on failure
  // (for example, if we don't currently have a RenderWidgetHostView.)
  BackingStore* AllocBackingStore(const gfx::Size& size);

  // When a backing store does asynchronous painting, it will call this function
  // when it is done with the DIB. We will then forward a message to the
  // renderer to send another paint.
  void DonePaintingToBackingStore();

  // GPU accelerated version of GetBackingStore function. This will
  // trigger a re-composite to the view. If a resize is pending, it will
  // block briefly waiting for an ack from the renderer.
  void ScheduleComposite();

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartHangMonitorTimeout(base::TimeDelta delay);

  // Restart the active hang monitor timeout. Clears all existing timeouts and
  // starts with a new one.  This can be because the renderer has become
  // active, the tab is being hidden, or the user has chosen to wait some more
  // to give the tab a chance to become active and we don't want to display a
  // warning too soon.
  void RestartHangMonitorTimeout();

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopHangMonitorTimeout();

  // Called when the system theme changes. At this time all existing native
  // theme handles are invalid and the renderer must obtain new ones and
  // repaint.
  void SystemThemeChanged();

  // Forwards the given message to the renderer. These are called by the view
  // when it has received a message.
  virtual void ForwardMouseEvent(const WebKit::WebMouseEvent& mouse_event);
  // Called when a mouse click activates the renderer.
  virtual void OnMouseActivate();
  void ForwardWheelEvent(const WebKit::WebMouseWheelEvent& wheel_event);
  virtual void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event);
  virtual void ForwardEditCommand(const std::string& name,
                                  const std::string& value);
  virtual void ForwardEditCommandsForNextKeyEvent(
      const EditCommands& edit_commands);
#if defined(TOUCH_UI)
  virtual void ForwardTouchEvent(const WebKit::WebTouchEvent& touch_event);
#endif


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
  void UpdateTextDirection(WebKit::WebTextDirection direction);
  void CancelUpdateTextDirection();
  void NotifyTextDirection();

  // Notifies the renderer whether or not the input method attached to this
  // process is activated.
  // When the input method is activated, a renderer process sends IPC messages
  // to notify the status of its composition node. (This message is mainly used
  // for notifying the position of the input cursor so that the browser can
  // display input method windows under the cursor.)
  void SetInputMethodActive(bool activate);

  // Update the composition node of the renderer (or WebKit).
  // WebKit has a special node (a composition node) for input method to change
  // its text without affecting any other DOM nodes. When the input method
  // (attached to the browser) updates its text, the browser sends IPC messages
  // to update the composition node of the renderer.
  // (Read the comments of each function for its detail.)

  // Sets the text of the composition node.
  // This function can also update the cursor position and mark the specified
  // range in the composition node.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_COMPSTR flag
  //   (on Windows);
  // * when it receives a "preedit_changed" signal of GtkIMContext (on Linux);
  // * when markedText of NSTextInput is called (on Mac).
  void ImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);

  // Finishes an ongoing composition with the specified text.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeConfirmComposition(const string16& text);

  // Finishes an ongoing composition with the composition text set by last
  // SetComposition() call.
  void ImeConfirmComposition();

  // Cancels an ongoing composition.
  void ImeCancelComposition();

  // Makes an IPC call to toggle the spelling panel.
  void ToggleSpellPanel(bool is_currently_visible);

  // Makes an IPC call to tell webkit to replace the currently selected word
  // or a word around the cursor.
  void Replace(const string16& word);

  // Makes an IPC call to tell webkit to advance to the next misspelling.
  void AdvanceToNextMisspelling();

  // Enable renderer accessibility. This should only be called when a
  // screenreader is detected.
  void EnableRendererAccessibility();

  // Relays a request from assistive technology to set focus to the
  // node with this accessibility object id.
  void SetAccessibilityFocus(int acc_obj_id);

  // Relays a request from assistive technology to perform the default action
  // on a node with this accessibility object id.
  void AccessibilityDoDefaultAction(int acc_obj_id);

  // Acknowledges a ViewHostMsg_AccessibilityNotifications message.
  void AccessibilityNotificationsAck();

  // Sets the active state (i.e., control tints).
  virtual void SetActive(bool active);

  void set_ignore_input_events(bool ignore_input_events) {
    ignore_input_events_ = ignore_input_events;
  }
  bool ignore_input_events() const {
    return ignore_input_events_;
  }

  // Activate deferred plugin handles.
  void ActivateDeferredPluginHandles();

  const gfx::Point& last_scroll_offset() const { return last_scroll_offset_; }

 protected:
  // Internal implementation of the public Forward*Event() methods.
  void ForwardInputEvent(const WebKit::WebInputEvent& input_event,
                         int event_size, bool is_keyboard_shortcut);

  // Called when we receive a notification indicating that the renderer
  // process has gone. This will reset our state so that our state will be
  // consistent if a new renderer is created.
  void RendererExited(base::TerminationStatus status, int exit_code);

  // Retrieves an id the renderer can use to refer to its view.
  // This is used for various IPC messages, including plugins.
  gfx::NativeViewId GetNativeViewId();

  // Called to handled a keyboard event before sending it to the renderer.
  // This is overridden by RenderView to send upwards to its delegate.
  // Returns true if the event was handled, and then the keyboard event will
  // not be sent to the renderer anymore. Otherwise, if the |event| would
  // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);

  // Called when a keyboard event was not processed by the renderer. This is
  // overridden by RenderView to send upwards to its delegate.
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Notification that the user has made some kind of input that could
  // perform an action. The render view host overrides this to forward the
  // information to its delegate (see corresponding function in
  // RenderViewHostDelegate). The gestures that count are 1) any mouse down
  // event and 2) enter or space key presses.
  virtual void OnUserGesture() {}

  // Callbacks for notification when the renderer becomes unresponsive to user
  // input events, and subsequently responsive again. RenderViewHost overrides
  // these to tell its delegate to show the user a warning.
  virtual void NotifyRendererUnresponsive() {}
  virtual void NotifyRendererResponsive() {}

 protected:
  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, Resize);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, ResizeThenCrash);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, HiddenPaint);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, PaintAtSize);

  // Tell this object to destroy itself.
  void Destroy();

  // Checks whether the renderer is hung and calls NotifyRendererUnresponsive
  // if it is.
  void CheckRendererIsUnresponsive();

  // Called if we know the renderer is responsive. When we currently think the
  // renderer is unresponsive, this will clear that state and call
  // NotifyRendererResponsive.
  void RendererIsResponsive();

  // IPC message handlers
  void OnMsgRenderViewReady();
  void OnMsgRenderViewGone(int status, int error_code);
  void OnMsgClose();
  void OnMsgRequestMove(const gfx::Rect& pos);
  void OnMsgPaintAtSizeAck(int tag, const gfx::Size& size);
  void OnMsgUpdateRect(const ViewHostMsg_UpdateRect_Params& params);
  void OnMsgInputEventAck(const IPC::Message& message);
  virtual void OnMsgFocus();
  virtual void OnMsgBlur();

  void OnMsgSetCursor(const WebCursor& cursor);
  void OnMsgImeUpdateTextInputState(WebKit::WebTextInputType type,
                                    const gfx::Rect& caret_rect);
  void OnMsgImeCancelComposition();

  void OnMsgDidActivateAcceleratedCompositing(bool activated);

#if defined(OS_MACOSX)
  void OnMsgGetScreenInfo(gfx::NativeViewId view,
                          WebKit::WebScreenInfo* results);
  void OnMsgGetWindowRect(gfx::NativeViewId window_id, gfx::Rect* results);
  void OnMsgGetRootWindowRect(gfx::NativeViewId window_id, gfx::Rect* results);
  void OnMsgPluginFocusChanged(bool focused, int plugin_id);
  void OnMsgStartPluginIme();
  void OnAllocateFakePluginWindowHandle(bool opaque,
                                        bool root,
                                        gfx::PluginWindowHandle* id);
  void OnDestroyFakePluginWindowHandle(gfx::PluginWindowHandle id);
  void OnAcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                        int32 width,
                                        int32 height,
                                        uint64 mach_port);
  void OnAcceleratedSurfaceSetTransportDIB(gfx::PluginWindowHandle window,
                                           int32 width,
                                           int32 height,
                                           TransportDIB::Handle transport_dib);
  void OnAcceleratedSurfaceBuffersSwapped(gfx::PluginWindowHandle window,
                                          uint64 surface_id);
#elif defined(OS_POSIX)
  void OnMsgCreatePluginContainer(gfx::PluginWindowHandle id);
  void OnMsgDestroyPluginContainer(gfx::PluginWindowHandle id);
#endif

  // Paints the given bitmap to the current backing store at the given location.
  void PaintBackingStoreRect(TransportDIB::Id bitmap,
                             const gfx::Rect& bitmap_rect,
                             const std::vector<gfx::Rect>& copy_rects,
                             const gfx::Size& view_size);

  // Scrolls the given |clip_rect| in the backing by the given dx/dy amount. The
  // |dib| and its corresponding location |bitmap_rect| in the backing store
  // is the newly painted pixels by the renderer.
  void ScrollBackingStoreRect(int dx, int dy, const gfx::Rect& clip_rect,
                              const gfx::Size& view_size);

  // Called by OnMsgInputEventAck() to process a keyboard event ack message.
  void ProcessKeyboardEventAck(int type, bool processed);

  // Called by OnMsgInputEventAck() to process a wheel event ack message.
  // This could result in a task being posted to allow additional wheel
  // input messages to be coalesced.
  void ProcessWheelAck();

  // True if renderer accessibility is enabled. This should only be set when a
  // screenreader is detected as it can potentially slow down Chrome.
  bool renderer_accessible_;

  // The View associated with the RenderViewHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  RenderWidgetHostView* view_;

  // Created during construction but initialized during Init*(). Therefore, it
  // is guaranteed never to be NULL, but its channel may be NULL if the
  // renderer crashed, so you must always check that.
  RenderProcessHost* process_;

  // Stores random bits of data for others to associate with this object.
  PropertyBag property_bag_;

  // The ID of the corresponding object in the Renderer Instance.
  int routing_id_;

  // Indicates whether a page is loading or not.
  bool is_loading_;

  // Indicates whether a page is hidden or not.
  bool is_hidden_;

  // True when a page is rendered directly via the GPU process.
  bool is_accelerated_compositing_active_;

  // Set if we are waiting for a repaint ack for the view.
  bool repaint_ack_pending_;

  // True when waiting for RESIZE_ACK.
  bool resize_ack_pending_;

  // The current size of the RenderWidget.
  gfx::Size current_size_;

  // The current reserved area of the RenderWidget where contents should not be
  // rendered to draw the resize corner, sidebar mini tabs etc.
  gfx::Rect current_reserved_rect_;

  // The size we last sent as requested size to the renderer. |current_size_|
  // is only updated once the resize message has been ack'd. This on the other
  // hand is updated when the resize message is sent. This is very similar to
  // |resize_ack_pending_|, but the latter is not set if the new size has width
  // or height zero, which is why we need this too.
  gfx::Size in_flight_size_;

  // The reserved area we last sent to the renderer. |current_reserved_rect_|
  // is only updated once the resize message has been ack'd. This on the other
  // hand is updated when the resize message is sent.
  gfx::Rect in_flight_reserved_rect_;

  // True if a mouse move event was sent to the render view and we are waiting
  // for a corresponding ViewHostMsg_HandleInputEvent_ACK message.
  bool mouse_move_pending_;

  // The next mouse move event to send (only non-null while mouse_move_pending_
  // is true).
  scoped_ptr<WebKit::WebMouseEvent> next_mouse_move_;

  // (Similar to |mouse_move_pending_|.) True if a mouse wheel event was sent
  // and we are waiting for a corresponding ack.
  bool mouse_wheel_pending_;

  typedef std::deque<WebKit::WebMouseWheelEvent> WheelEventQueue;

  // (Similar to |next_mouse_move_|.) The next mouse wheel events to send.
  // Unlike mouse moves, mouse wheel events received while one is pending are
  // coalesced (by accumulating deltas) if they match the previous event in
  // modifiers. On the Mac, in particular, mouse wheel events are received at a
  // high rate; not waiting for the ack results in jankiness, and using the same
  // mechanism as for mouse moves (just dropping old events when multiple ones
  // would be queued) results in very slow scrolling.
  WheelEventQueue coalesced_mouse_wheel_events_;

  // The time when an input event was sent to the RenderWidget.
  base::TimeTicks input_event_start_time_;

  // If true, then we should repaint when restoring even if we have a
  // backingstore.  This flag is set to true if we receive a paint message
  // while is_hidden_ to true.  Even though we tell the render widget to hide
  // itself, a paint message could already be in flight at that point.
  bool needs_repainting_on_restore_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_;

  // The following value indicates a time in the future when we would consider
  // the renderer hung if it does not generate an appropriate response message.
  base::Time time_when_considered_hung_;

  // This timer runs to check if time_when_considered_hung_ has past.
  base::OneShotTimer<RenderWidgetHost> hung_renderer_timer_;

  // Flag to detect recursive calls to GetBackingStore().
  bool in_get_backing_store_;

  // Set when we call DidPaintRect/DidScrollRect on the view.
  bool view_being_painted_;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  // Queue of keyboard events that we need to track.
  typedef std::deque<NativeWebKeyboardEvent> KeyQueue;

  // A queue of keyboard events. We can't trust data from the renderer so we
  // stuff key events into a queue and pop them out on ACK, feeding our copy
  // back to whatever unhandled handler instead of the returned version.
  KeyQueue key_queue_;

  // Set to true if we shouldn't send input events from the render widget.
  bool ignore_input_events_;

  // Set when we update the text direction of the selected input element.
  bool text_direction_updated_;
  WebKit::WebTextDirection text_direction_;

  // Set when we cancel updating the text direction.
  // This flag also ignores succeeding update requests until we call
  // NotifyTextDirection().
  bool text_direction_canceled_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  // System may translate a RawKeyDown event into zero or more Char events,
  // usually we send them to the renderer directly in sequence. However, If a
  // RawKeyDown event was not handled by the renderer but was handled by
  // our UnhandledKeyboardEvent() method, e.g. as an accelerator key, then we
  // shall not send the following sequence of Char events, which was generated
  // by this RawKeyDown event, to the renderer. Otherwise the renderer may
  // handle the Char events and cause unexpected behavior.
  // For example, pressing alt-2 may let the browser switch to the second tab,
  // but the Char event generated by alt-2 may also activate a HTML element
  // if its accesskey happens to be "2", then the user may get confused when
  // switching back to the original tab, because the content may already be
  // changed.
  bool suppress_next_char_events_;

  std::vector<gfx::PluginWindowHandle> deferred_plugin_handles_;

  // The last scroll offset of the render widget.
  gfx::Point last_scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
