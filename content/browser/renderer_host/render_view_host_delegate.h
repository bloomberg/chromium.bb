// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/view_type.h"
#include "ipc/ipc_channel.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "webkit/glue/window_open_disposition.h"

class GURL;
class RenderViewHost;
class SkBitmap;
class TabContents;
class WebKeyboardEvent;
struct ContextMenuParams;
struct GlobalRequestID;
struct NativeWebKeyboardEvent;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct WebDropData;
struct WebMenuItem;
struct WebPreferences;

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
struct FileChooserParams;
struct RendererPreferences;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by TabContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
//
class CONTENT_EXPORT RenderViewHostDelegate : public IPC::Channel::Listener {
 public:
  // View ----------------------------------------------------------------------
  // Functions that can be routed directly to a view-specific class.
  class CONTENT_EXPORT View {
   public:
    // The page is trying to open a new page (e.g. a popup window). The window
    // should be created associated with the given route, but it should not be
    // shown yet. That should happen in response to ShowCreatedWindow.
    // |params.window_container_type| describes the type of RenderViewHost
    // container that is requested -- in particular, the window.open call may
    // have specified 'background' and 'persistent' in the feature string.
    //
    // The passed |params.frame_name| parameter is the name parameter that was
    // passed to window.open(), and will be empty if none was passed.
    //
    // Note: this is not called "CreateWindow" because that will clash with
    // the Windows function which is actually a #define.
    virtual void CreateNewWindow(
        int route_id,
        const ViewHostMsg_CreateWindow_Params& params) = 0;

    // The page is trying to open a new widget (e.g. a select popup). The
    // widget should be created associated with the given route, but it should
    // not be shown yet. That should happen in response to ShowCreatedWidget.
    // |popup_type| indicates if the widget is a popup and what kind of popup it
    // is (select, autofill...).
    virtual void CreateNewWidget(int route_id,
                                 WebKit::WebPopupType popup_type) = 0;

    // Creates a full screen RenderWidget. Similar to above.
    virtual void CreateNewFullscreenWidget(int route_id) = 0;

    // Show a previously created page with the specified disposition and bounds.
    // The window is identified by the route_id passed to CreateNewWindow.
    //
    // Note: this is not called "ShowWindow" because that will clash with
    // the Windows function which is actually a #define.
    virtual void ShowCreatedWindow(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) = 0;

    // Show the newly created widget with the specified bounds.
    // The widget is identified by the route_id passed to CreateNewWidget.
    virtual void ShowCreatedWidget(int route_id,
                                   const gfx::Rect& initial_pos) = 0;

    // Show the newly created full screen widget. Similar to above.
    virtual void ShowCreatedFullscreenWidget(int route_id) = 0;

    // A context menu should be shown, to be built using the context information
    // provided in the supplied params.
    virtual void ShowContextMenu(const ContextMenuParams& params) = 0;

    // Shows a popup menu with the specified items.
    // This method should call RenderViewHost::DidSelectPopupMenuItemAt() or
    // RenderViewHost::DidCancelPopupMenu() ased on the user action.
    virtual void ShowPopupMenu(const gfx::Rect& bounds,
                               int item_height,
                               double item_font_size,
                               int selected_item,
                               const std::vector<WebMenuItem>& items,
                               bool right_aligned) = 0;

    // The user started dragging content of the specified type within the
    // RenderView. Contextual information about the dragged content is supplied
    // by WebDropData.
    virtual void StartDragging(const WebDropData& drop_data,
                               WebKit::WebDragOperationsMask allowed_ops,
                               const SkBitmap& image,
                               const gfx::Point& image_offset) = 0;

    // The page wants to update the mouse cursor during a drag & drop operation.
    // |operation| describes the current operation (none, move, copy, link.)
    virtual void UpdateDragCursor(WebKit::WebDragOperation operation) = 0;

    // Notification that view for this delegate got the focus.
    virtual void GotFocus() = 0;

    // Callback to inform the browser that the page is returning the focus to
    // the browser's chrome. If reverse is true, it means the focus was
    // retrieved by doing a Shift-Tab.
    virtual void TakeFocus(bool reverse) = 0;

   protected:
    virtual ~View() {}
  };

  // RendererManagerment -------------------------------------------------------
  // Functions for managing switching of Renderers. For TabContents, this is
  // implemented by the RenderViewHostManager

  class CONTENT_EXPORT RendererManagement {
   public:
    // Notification whether we should close the page, after an explicit call to
    // AttemptToClosePage.  This is called before a cross-site request or before
    // a tab/window is closed (as indicated by the first parameter) to allow the
    // appropriate renderer to approve or deny the request.  |proceed| indicates
    // whether the user chose to proceed.
    virtual void ShouldClosePage(bool for_cross_site_transition,
                                 bool proceed) = 0;

    // Called by ResourceDispatcherHost when a response for a pending cross-site
    // request is received.  The ResourceDispatcherHost will pause the response
    // until the onunload handler of the previous renderer is run.
    virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                     int new_request_id) = 0;

   protected:
    virtual ~RendererManagement() {}
  };

  // ---------------------------------------------------------------------------

  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual View* GetViewDelegate();
  virtual RendererManagement* GetRendererManagementDelegate();

  // IPC::Channel::Listener implementation.
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Gets the URL that is currently being displayed, if there is one.
  virtual const GURL& GetURL() const;

  // Return this object cast to a TabContents, if it is one. If the object is
  // not a TabContents, returns NULL. DEPRECATED: Be sure to include brettw and
  // jam as reviewers before you use this method. http://crbug.com/82582
  virtual TabContents* GetAsTabContents();

  // Return type of RenderView which is attached with this object.
  virtual content::ViewType GetRenderViewType() const = 0;

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The RenderView died somehow (crashed or was killed by the user).
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // The RenderView was navigated to a different page.
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params) {}

  // The state for the page changed and should be updated.
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state) {}

  // The page's title was changed and should be updated.
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const string16& title,
                           base::i18n::TextDirection title_direction) {}

  // The page's encoding was changed and should be updated.
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::string& encoding) {}

  // The destination URL has changed should be updated
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) {}

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // The RenderViewHost has been swapped out.
  virtual void SwappedOut(RenderViewHost* render_view_host) {}

  // The page is trying to move the RenderView's representation in the client.
  virtual void RequestMove(const gfx::Rect& new_bounds) {}

  // The RenderView began loading a new page. This corresponds to WebKit's
  // notion of the throbber starting.
  virtual void DidStartLoading() {}

  // The RenderView stopped loading a page. This corresponds to WebKit's
  // notion of the throbber stopping.
  virtual void DidStopLoading() {}

  // The pending page load was canceled.
  virtual void DidCancelLoading() {}

  // The RenderView made progress loading a page's top frame.
  // |progress| is a value between 0 (nothing loaded) to 1.0 (top frame
  // entirely loaded).
  virtual void DidChangeLoadProgress(double progress) {}

  // The RenderView's main frame document element is ready. This happens when
  // the document has finished parsing.
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) {}

  // The onload handler in the RenderView's main frame has completed.
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id) {}

  // The page wants to open a URL with the specified disposition.
  virtual void RequestOpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              int64 source_frame_id) {}

  // The page wants to transfer the request to a new renderer.
  virtual void RequestTransferURL(const GURL& url,
                                  const GURL& referrer,
                                  WindowOpenDisposition disposition,
                                  int64 source_frame_id,
                                  const GlobalRequestID& old_request_id) {}

  // A javascript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(const RenderViewHost* rvh,
                                    const string16& message,
                                    const string16& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) {}

  virtual void RunBeforeUnloadConfirm(const RenderViewHost* rvh,
                                      const string16& message,
                                      IPC::Message* reply_msg) {}

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual content::RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const = 0;

  // Returns a WebPreferences object that will be used by the renderer
  // associated with the owning render view host.
  virtual WebPreferences GetWebkitPrefs();

  // Notification the user has made a gesture while focus was on the
  // page. This is used to avoid uninitiated user downloads (aka carpet
  // bombing), see DownloadRequestLimiter for details.
  virtual void OnUserGesture() {}

  // Notification from the renderer host that blocked UI event occurred.
  // This happens when there are tab-modal dialogs. In this case, the
  // notification is needed to let us draw attention to the dialog (i.e.
  // refocus on the modal dialog, flash title etc).
  virtual void OnIgnoredUIEvent() {}

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderViewHost* render_view_host) {}

  // Notification that the RenderViewHost's load state changed.
  virtual void LoadStateChanged(const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64 upload_position,
                                uint64 upload_size) {}

  // Notification that a worker process has crashed.
  virtual void WorkerCrashed() {}

  // The page wants the hosting window to activate/deactivate itself (it
  // called the JavaScript window.focus()/blur() method).
  virtual void Activate() {}
  virtual void Deactivate() {}

  // Notification that the view has lost capture.
  virtual void LostCapture() {}

  // Callback to give the browser a chance to handle the specified keyboard
  // event before sending it to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would
  // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);

  // Callback to inform the browser that the renderer did not process the
  // specified events. This gives an opportunity to the browser to process the
  // event (used for keyboard shortcuts).
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Notifications about mouse events in this view.  This is useful for
  // implementing global 'on hover' features external to the view.
  virtual void HandleMouseMove() {}
  virtual void HandleMouseDown() {}
  virtual void HandleMouseLeave() {}
  virtual void HandleMouseUp() {}
  virtual void HandleMouseActivate() {}

  // Called when a file selection is to be done.
  virtual void RunFileChooser(
      RenderViewHost* render_view_host,
      const content::FileChooserParams& params) {}

  // Notification that the page wants to go into or out of fullscreen mode.
  virtual void ToggleFullscreenMode(bool enter_fullscreen) {}
  virtual bool IsFullscreenForCurrentTab() const;

  // The contents' preferred size changed.
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  // Notification message from HTML UI.
  virtual void WebUISend(RenderViewHost* render_view_host,
                         const GURL& source_url,
                         const std::string& name,
                         const base::ListValue& args) {}
  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting render
  // view host.
  virtual void RequestToLockMouse() {}

  // Notification that the view has lost the mouse lock.
  virtual void LostMouseLock() {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
