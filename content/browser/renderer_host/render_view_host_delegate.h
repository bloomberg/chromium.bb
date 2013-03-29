// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/context_menu_source_type.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/media_stream_request.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SkBitmap;
class WebKeyboardEvent;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_DidFailProvisionalLoadWithError_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewMsg_PostMessage_Params;

namespace webkit_glue {
struct WebPreferences;
}

namespace base {
class ListValue;
class TimeTicks;
}

namespace IPC {
class Message;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace content {

class BrowserContext;
class RenderViewHost;
class RenderViewHostDelegateView;
class SessionStorageNamespace;
class WebContents;
class WebContentsImpl;
struct ContextMenuParams;
struct FileChooserParams;
struct GlobalRequestID;
struct NativeWebKeyboardEvent;
struct Referrer;
struct RendererPreferences;

typedef base::Callback< void(const MediaStreamDevices&) > MediaResponseCallback;

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by WebContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
class CONTENT_EXPORT RenderViewHostDelegate {
 public:
  // RendererManagerment -------------------------------------------------------
  // Functions for managing switching of Renderers. For WebContents, this is
  // implemented by the RenderViewHostManager.

  class CONTENT_EXPORT RendererManagement {
   public:
    // Notification whether we should close the page, after an explicit call to
    // AttemptToClosePage.  This is called before a cross-site request or before
    // a tab/window is closed (as indicated by the first parameter) to allow the
    // appropriate renderer to approve or deny the request.  |proceed| indicates
    // whether the user chose to proceed.  |proceed_time| is the time when the
    // request was allowed to proceed.
    virtual void ShouldClosePage(
        bool for_cross_site_transition,
        bool proceed,
        const base::TimeTicks& proceed_time) = 0;

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
  virtual RenderViewHostDelegateView* GetDelegateView();
  virtual RendererManagement* GetRendererManagementDelegate();

  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderViewHost* render_view_host,
                                 const IPC::Message& message);

  // Gets the URL that is currently being displayed, if there is one.
  virtual const GURL& GetURL() const;

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL. DEPRECATED: Be sure to include brettw or
  // jam as reviewers before you use this method. http://crbug.com/82582
  virtual WebContents* GetAsWebContents();

  // Return the rect where to display the resize corner, if any, otherwise
  // an empty rect.
  virtual gfx::Rect GetRootWindowResizerRect() const = 0;

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

  // The RenderView started a provisional load for a given frame.
  virtual void DidStartProvisionalLoadForFrame(
      RenderViewHost* render_view_host,
      int64 frame_id,
      int64 parent_frame_id,
      bool main_frame,
      const GURL& url) {}

  // The RenderView processed a redirect during a provisional load.
  //
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // the ResourceDispatcherHost's RESOURCE_RECEIVED_REDIRECT notification
  // instead.  See http://crbug.com/78512.
  virtual void DidRedirectProvisionalLoad(
      RenderViewHost* render_view_host,
      int32 page_id,
      const GURL& source_url,
      const GURL& target_url) {}

  // A provisional load in the RenderView failed.
  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params) {}

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
  virtual void DidStartLoading(RenderViewHost* render_view_host) {}

  // The RenderView stopped loading a page. This corresponds to WebKit's
  // notion of the throbber stopping.
  virtual void DidStopLoading(RenderViewHost* render_view_host) {}

  // The pending page load was canceled.
  virtual void DidCancelLoading() {}

  // The RenderView made progress loading a page's top frame.
  // |progress| is a value between 0 (nothing loaded) to 1.0 (top frame
  // entirely loaded).
  virtual void DidChangeLoadProgress(double progress) {}

  // The RenderView set its opener to null, disowning it for the lifetime of
  // the window.
  virtual void DidDisownOpener(RenderViewHost* rvh) {}

  // Another page accessed the initial empty document of this RenderView,
  // which means it is no longer safe to display a pending URL without
  // risking a URL spoof.
  virtual void DidAccessInitialDocument() {}

  // The RenderView has changed its frame hierarchy, so we need to update all
  // other renderers interested in this event.
  virtual void DidUpdateFrameTree(RenderViewHost* rvh) {}

  // The RenderView's main frame document element is ready. This happens when
  // the document has finished parsing.
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) {}

  // The onload handler in the RenderView's main frame has completed.
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id) {}

  // The page wants to open a URL with the specified disposition.
  virtual void RequestOpenURL(RenderViewHost* rvh,
                              const GURL& url,
                              const Referrer& referrer,
                              WindowOpenDisposition disposition,
                              int64 source_frame_id,
                              bool is_redirect) {}

  // The page wants to transfer the request to a new renderer.
  virtual void RequestTransferURL(
      const GURL& url,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      int64 source_frame_id,
      const GlobalRequestID& old_request_id,
      bool is_redirect) {}

  // The page wants to close the active view in this tab.
  virtual void RouteCloseEvent(RenderViewHost* rvh) {}

  // The page wants to post a message to the active view in this tab.
  virtual void RouteMessageEvent(
      RenderViewHost* rvh,
      const ViewMsg_PostMessage_Params& params) {}

  // A javascript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(RenderViewHost* rvh,
                                    const string16& message,
                                    const string16& default_prompt,
                                    const GURL& frame_url,
                                    JavaScriptMessageType type,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) {}

  virtual void RunBeforeUnloadConfirm(RenderViewHost* rvh,
                                      const string16& message,
                                      bool is_reload,
                                      IPC::Message* reply_msg) {}

  // A message was added to to the console.
  virtual bool AddMessageToConsole(int32 level,
                                   const string16& message,
                                   int32 line_no,
                                   const string16& source_id);

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const = 0;

  // Returns a WebPreferences object that will be used by the renderer
  // associated with the owning render view host.
  virtual webkit_glue::WebPreferences GetWebkitPrefs();

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

  // Notifications about mouse events in this view.  This is useful for
  // implementing global 'on hover' features external to the view.
  virtual void HandleMouseMove() {}
  virtual void HandleMouseDown() {}
  virtual void HandleMouseLeave() {}
  virtual void HandleMouseUp() {}
  virtual void HandlePointerActivate() {}
  virtual void HandleGestureBegin() {}
  virtual void HandleGestureEnd() {}

  // Called when a file selection is to be done.
  virtual void RunFileChooser(
      RenderViewHost* render_view_host,
      const FileChooserParams& params) {}

  // Notification that the page wants to go into or out of fullscreen mode.
  virtual void ToggleFullscreenMode(bool enter_fullscreen) {}
  virtual bool IsFullscreenForCurrentTab() const;

  // The contents' preferred size changed.
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  // The contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting render
  // view host.
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target) {}

  // Notification that the view has lost the mouse lock.
  virtual void LostMouseLock() {}

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
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) {}

  // The page is trying to open a new widget (e.g. a select popup). The
  // widget should be created associated with the given route, but it should
  // not be shown yet. That should happen in response to ShowCreatedWidget.
  // |popup_type| indicates if the widget is a popup and what kind of popup it
  // is (select, autofill...).
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) {}

  // Creates a full screen RenderWidget. Similar to above.
  virtual void CreateNewFullscreenWidget(int route_id) {}

  // Show a previously created page with the specified disposition and bounds.
  // The window is identified by the route_id passed to CreateNewWindow.
  //
  // Note: this is not called "ShowWindow" because that will clash with
  // the Windows function which is actually a #define.
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {}

  // Show the newly created widget with the specified bounds.
  // The widget is identified by the route_id passed to CreateNewWidget.
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}

  // Show the newly created full screen widget. Similar to above.
  virtual void ShowCreatedFullscreenWidget(int route_id) {}

  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(const ContextMenuParams& params,
                               ContextMenuSourceType type) {}

  // The render view has requested access to media devices listed in
  // |request|, and the client should grant or deny that permission by
  // calling |callback|.
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback) {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
