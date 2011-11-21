// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
#pragma once

// This file describes various types used to describe and filter notifications
// that pass through the NotificationService.
//
namespace content {

enum NotificationType {
  NOTIFICATION_CONTENT_START = 0,

  // General -----------------------------------------------------------------

  // Special signal value to represent an interest in all notifications.
  // Not valid when posting a notification.
  NOTIFICATION_ALL = NOTIFICATION_CONTENT_START,

  // The app is done processing user actions, now is a good time to do
  // some background work.
  NOTIFICATION_IDLE,

  // Means that the app has just started doing something in response to a
  // user action, and that background processes shouldn't run if avoidable.
  NOTIFICATION_BUSY,

  // This is sent when the user does a gesture resulting in a noteworthy
  // action taking place. This is typically used for logging. The source is
  // the browser context, and the details is a string identifying the action.
  NOTIFICATION_USER_ACTION,

  // NavigationController ----------------------------------------------------

  // A new pending navigation has been created. Pending entries are created
  // when the user requests the navigation. We don't know if it will actually
  // happen until it does (at this point, it will be "committed." Note that
  // renderer- initiated navigations such as link clicks will never be
  // pending.
  //
  // This notification is called after the pending entry is created, but
  // before we actually try to navigate. The source will be the
  // NavigationController that owns the pending entry, and the details
  // will be a NavigationEntry.
  NOTIFICATION_NAV_ENTRY_PENDING,

  // A new non-pending navigation entry has been created. This will
  // correspond to one NavigationController entry being created (in the case
  // of new navigations) or renavigated to (for back/forward navigations).
  //
  // The source will be the navigation controller doing the commit. The
  // details will be NavigationController::LoadCommittedDetails.
  NOTIFICATION_NAV_ENTRY_COMMITTED,

  // Indicates that the NavigationController given in the Source has
  // decreased its back/forward list count by removing entries from either
  // the front or back of its list. This is usually the result of going back
  // and then doing a new navigation, meaning all the "forward" items are
  // deleted.
  //
  // This normally happens as a result of a new navigation. It will be
  // followed by a NAV_ENTRY_COMMITTED message for the new page that
  // caused the pruning. It could also be a result of removing an item from
  // the list to fix up after interstitials.
  //
  // The details are NavigationController::PrunedDetails.
  NOTIFICATION_NAV_LIST_PRUNED,

  // Indicates that a NavigationEntry has changed. The source will be the
  // NavigationController that owns the NavigationEntry. The details will be
  // a NavigationController::EntryChangedDetails struct.
  //
  // This will NOT be sent on navigation, interested parties should also
  // listen for NAV_ENTRY_COMMITTED to handle that case. This will be
  // sent when the entry is updated outside of navigation (like when a new
  // title comes).
  NOTIFICATION_NAV_ENTRY_CHANGED,

  // Other load-related (not from NavigationController) ----------------------

  // Corresponds to ViewHostMsg_DocumentOnLoadCompletedInMainFrame. The source
  // is the TabContents and the details the page_id.
  NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,

  // A content load is starting.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  No details are expected for this notification.
  NOTIFICATION_LOAD_START,

  // A content load has stopped. The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  Details in the form of a LoadNotificationDetails object
  // are optional.
  NOTIFICATION_LOAD_STOP,

  // Content was loaded from an in-memory cache.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // occurred.  Details in the form of a LoadFromMemoryCacheDetails object
  // are provided.
  NOTIFICATION_LOAD_FROM_MEMORY_CACHE,

  // A provisional content load has failed with an error.  The source will be
  // a Source<NavigationController> corresponding to the tab in which the
  // load occurred.  Details in the form of a ProvisionalLoadDetails object
  // are provided.
  NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR,

  // A response has been received for a resource request.  The source will be
  // a Source<RenderViewHostDelegate> corresponding to the tab in which the
  // request was issued.  Details in the form of a ResourceRequestDetails
  // object are provided.
  NOTIFICATION_RESOURCE_RESPONSE_STARTED,

  // A redirect was received while requesting a resource.  The source will be
  // a Source<RenderViewHostDelegate> corresponding to the tab in which the
  // request was issued.  Details in the form of a ResourceRedirectDetails
  // are provided.
  NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,

  // A new tab is created from an existing tab to serve as a target of a
  // navigation that is about to happen. The source will be a
  // Source<BrowserContext> corresponding to the browser context in which the
  // new tab will live. Details in the form of a RetargetingDetails object are
  // provided.
  NOTIFICATION_RETARGETING,

  // A new window was requested but was not created. The source will be a
  // Source<TabContents> corresponding to the tab the request originated from.
  // Details are the ViewHostMsg_CreateWindow_Params object that were used in
  // the request.
  NOTIFICATION_CREATING_NEW_WINDOW_CANCELLED,

  // SSL ---------------------------------------------------------------------

  // Updating the SSL security indicators (the lock icon and such) proceeds
  // in two phases:
  //
  // 1) The internal SSL state for a host or tab changes.  When this happens,
  //    the SSLManager broadcasts an SSL_INTERNAL_STATE_CHANGED notification.
  //
  // 2) The SSLManager for each tab receives this notification and might or
  //    might not update the navigation entry for its tab, depending on
  //    whether the change in state affects that tab.  If the SSLManager does
  //    change the navigation entry, then the SSLManager broadcasts an
  //    SSL_VISIBLE_STATE_CHANGED notification to the user interface can
  //    redraw properly.

  // The SSL state of a page has changed in some visible way.  For example,
  // if an insecure resource is loaded on a secure page.  Note that a
  // toplevel load commit will also update the SSL state (since the
  // NavigationEntry is new) and this message won't always be sent in that
  // case.  Listen to this notification if you need to refresh SSL-related UI
  // elements.
  //
  // There is no source or details.
  NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,

  // The SSL state of the browser has changed in some internal way.  For
  // example, the user might have explicitly allowed some broken certificate
  // or a secure origin might have included some insecure content.  Listen to
  // this notifiation if you need to keep track of our internal SSL state.
  //
  // The source will be the browser context. The details will be the navigation
  // controller associated with the state change.
  NOTIFICATION_SSL_INTERNAL_STATE_CHANGED,

  // The user accepted or dismissed a SSL client authentication request.
  // The source is a Source<SSLClientAuthHandler>.  Details is a
  // SSLClientAuthNotificationDetails which records specifies which
  // SSLCertRequestInfo the request was for and which X509Certificate was
  // selected (if any).
  NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,

#if defined(OS_MACOSX)
  // This message is sent when the application is made active (Mac OS X only
  // at present). No source or details are passed.
  NOTIFICATION_APP_ACTIVATED,
#endif

  // This message is sent when the application is terminating (the last
  // browser window has shutdown as part of an explicit user-initiated exit,
  // or the user closed the last browser window on Windows/Linux and there are
  // no BackgroundContents keeping the browser running). No source or details
  // are passed.
  NOTIFICATION_APP_TERMINATING,

#if defined(OS_MACOSX)
  // This notification is sent when the app has no key window, such as when
  // all windows are closed but the app is still active. No source or details
  // are provided.
  NOTIFICATION_NO_KEY_WINDOW,
#endif

  // This is sent when the user has chosen to exit the app, but before any
  // browsers have closed. This is sent if the user chooses to exit
  // (via exit menu item or keyboard shortcut) or to restart the process
  // (such as in flags page), not if Chrome exists by some other means
  // (such as the user closing the last window). Note that receiving this
  // notification does not necessarily mean the process will exit
  // because the shutdown process can be cancelled by unload handler.
  // Use APP_TERMINATING for such needs.
  //  The source and details are unspecified.
  NOTIFICATION_APP_EXITING,

  // Indicates that a devtools window is closing. The source is the
  // content::BrowserContext* and the details is the inspected RenderViewHost*.
  NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,

  // Tabs --------------------------------------------------------------------

  // Sent when a tab is added to a TabContentsDelegate. The source is the
  // TabContentsDelegate and the details is the added TabContents.
  NOTIFICATION_TAB_ADDED,

  // This notification is sent after a tab has been appended to the tab_strip.
  // The source is a Source<TabContentsWrapper> of the tab being added. There
  // are no details.
  NOTIFICATION_TAB_PARENTED,

  // This message is sent before a tab has been closed.  The source is a
  // Source<NavigationController> with a pointer to the controller for the
  // closed tab.  No details are expected.
  //
  // See also TAB_CLOSED.
  NOTIFICATION_TAB_CLOSING,

  // Notification that a tab has been closed. The source is the
  // NavigationController with no details.
  NOTIFICATION_TAB_CLOSED,

  // This notification is sent when a render view host has connected to a
  // renderer process. The source is a Source<TabContents> with a pointer to
  // the TabContents.  A TAB_CONTENTS_DISCONNECTED notification is
  // guaranteed before the source pointer becomes junk.  No details are
  // expected.
  NOTIFICATION_TAB_CONTENTS_CONNECTED,

  // This notification is sent when a TabContents swaps its render view host
  // with another one, possibly changing processes. The source is a
  // Source<TabContents> with a pointer to the TabContents.  A
  // TAB_CONTENTS_DISCONNECTED notification is guaranteed before the
  // source pointer becomes junk.  No details are expected.
  NOTIFICATION_TAB_CONTENTS_SWAPPED,

  // This message is sent after a TabContents is disconnected from the
  // renderer process.  The source is a Source<TabContents> with a pointer to
  // the TabContents (the pointer is usable).  No details are expected.
  NOTIFICATION_TAB_CONTENTS_DISCONNECTED,

  // This notification is sent after TabContents' title is updated. The source
  // is a Source<TabContents> with a pointer to the TabContents. The details
  // is a Details<TitleUpdatedDetails> that contains more information.
  NOTIFICATION_TAB_CONTENTS_TITLE_UPDATED,

  // This notification is sent when a TabContents is being hidden, e.g. due
  // to switching away from this tab.  The source is a Source<TabContents>.
  NOTIFICATION_TAB_CONTENTS_HIDDEN,

  // This notification is sent when a TabContents is being destroyed. Any
  // object holding a reference to a TabContents can listen to that
  // notification to properly reset the reference. The source is a
  // Source<TabContents>.
  NOTIFICATION_TAB_CONTENTS_DESTROYED,

  // A RenderViewHost was created for a TabContents. The source is the
  // associated TabContents, and the details is the RenderViewHost
  // pointer.
  NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,

  // Notification than an interstitial has become associated with a tab. The
  // source is the TabContents, the details not used.
  NOTIFICATION_INTERSTITIAL_ATTACHED,

  // Indicates that a RenderProcessHost was created and its handle is now
  // available. The source will be the RenderProcessHost that corresponds to
  // the process.
  NOTIFICATION_RENDERER_PROCESS_CREATED,

  // Indicates that a RenderProcessHost is destructing. The source will be the
  // RenderProcessHost that corresponds to the process.
  NOTIFICATION_RENDERER_PROCESS_TERMINATED,

  // Indicates that a render process is starting to exit, such that it should
  // not be used for future navigations.  The source will be the
  // RenderProcessHost that corresponds to the process.
  NOTIFICATION_RENDERER_PROCESS_CLOSING,

  // Indicates that a render process was closed (meaning it exited, but the
  // RenderProcessHost might be reused).  The source will be the corresponding
  // RenderProcessHost.  The details will be a RendererClosedDetails struct.
  // This may get sent along with RENDERER_PROCESS_TERMINATED.
  NOTIFICATION_RENDERER_PROCESS_CLOSED,

  // Indicates that a render process has become unresponsive for a period of
  // time. The source will be the RenderWidgetHost that corresponds to the
  // hung view, and no details are expected.
  NOTIFICATION_RENDERER_PROCESS_HANG,

  // This is sent to notify that the RenderViewHost displayed in a
  // TabContents has changed.  Source is the TabContents for which the change
  // happened, details is the previous RenderViewHost (can be NULL when the
  // first RenderViewHost is set).
  NOTIFICATION_RENDER_VIEW_HOST_CHANGED,

  // Indicates that the render view host has received an accessibility tree
  // update, either partial or full, from the render view.  The source is the
  // RenderViewHost, the details are not used.
  NOTIFICATION_RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,

  // This is sent when a RenderWidgetHost is being destroyed. The source is
  // the RenderWidgetHost, the details are not used.
  NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,

  // Sent after the widget has painted. The source is the RenderWidgetHost,
  // the details are not used.
  NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,

  // This notifies the observer that a PaintAtSizeACK was received. The source
  // is the RenderWidgetHost, the details are an instance of
  // RenderWidgetHost::PaintAtSizeAckDetails.
  NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,

  // This notifies the observer that a HandleInputEventACK was received. The
  // source is the RenderWidgetHost, the details are the type of event
  // received.
  // Note: The RenderWidgetHost may be deallocated at this point.
  // Used only in testing.
  NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,

  // Sent from RenderViewHost constructor. The source is the RenderViewHost,
  // the details unused.
  NOTIFICATION_RENDER_VIEW_HOST_CREATED,

  // Sent from ~RenderViewHost. The source is the RenderViewHost, the details
  // unused.
  NOTIFICATION_RENDER_VIEW_HOST_DELETED,

  // Sent from RenderViewHost::ClosePage.  The hosted RenderView has
  // processed the onbeforeunload handler and is about to be sent a
  // ViewMsg_ClosePage message to complete the tear-down process.  The source
  // is the RenderViewHost sending the message, and no details are provided.
  // Note:  This message is not sent in response to RenderView closure
  // initiated by window.close().
  NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,

  // This notifies the observer that the drag operation ack in a drag and
  // drop operation was received. The source is the RenderViewHost.
  // Note: Used only in testing.
  NOTIFICATION_RENDER_VIEW_HOST_DID_RECEIVE_DRAG_TARGET_DROP_ACK,

  // Indicates a RenderWidgetHost has been hidden or restored. The source is
  // the RWH whose visibility changed, the details is a bool set to true if
  // the new state is "visible."
  NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,

  // The focused element inside a page has changed.  The source is the
  // TabContents containing the render view host for the page. The details is
  // a Details<const bool> that indicates whether or not an editable node was
  // focused.
  NOTIFICATION_FOCUS_CHANGED_IN_PAGE,

  // Notification posted from ExecuteJavascriptInWebFrameNotifyResult. The
  // source is the RenderViewHost ExecuteJavascriptInWebFrameNotifyResult was
  // invoked on. The details are a std::pair<int, Value*> with the int giving
  // the id returned from ExecuteJavascriptInWebFrameNotifyResult and the
  // Value the results of the javascript expression. The Value is owned by
  // RenderViewHost and may be a Null Value.
  NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,

  // Child Processes ---------------------------------------------------------

  // This notification is sent when a child process host has connected to a
  // child process.  There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<ChildProcessInfo>.
  NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,

  // This message is sent after a ChildProcessHost is disconnected from the
  // child process.  There is no usable source, since it is sent from an
  // ephemeral task; register for AllSources() to receive this notification.
  // The details are in a Details<ChildProcessInfo>.
  NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED,

  // This message is sent when a child process disappears
  // unexpectedly as a result of a crash.  There is no usable
  // source, since it is sent from an ephemeral task; register for
  // AllSources() to receive this notification.  The details are in
  // a Details<ChildProcessInfo>.
  NOTIFICATION_CHILD_PROCESS_CRASHED,

  // This message is sent when a child process disappears
  // unexpectedly as a result of a termination signal.  There is no
  // usable source, since it is sent from an ephemeral task;
  // register for AllSources() to receive this notification.  The
  // details are in a Details<ChildProcessInfo>.
  NOTIFICATION_CHILD_PROCESS_WAS_KILLED,

  // This message indicates that an instance of a particular child was
  // created in a page.  (If one page contains several regions rendered by
  // the same child, this notification will occur once for each region
  // during the page load.)
  //
  // There is no usable source, since it is sent from an ephemeral task;
  // register for AllSources() to receive this notification.  The details are
  // in a Details<ChildProcessInfo>.
  NOTIFICATION_CHILD_INSTANCE_CREATED,

  // Download Notifications --------------------------------------------------

  // Sent when a page generation to MHTML has finished.
  // The source is the corresponding RenderViewHost. The details is a
  // MHTMLGenerationManager::NotificationDetails.
  NOTIFICATION_MHTML_GENERATED,

  // Saved Pages -------------------------------------------------------------

  // Sent when a SavePackage finishes successfully. The source is the
  // SavePackage, and Details are a GURL containing address of downloaded
  // page.
  NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,

  // Purge Memory ------------------------------------------------------------

  // Sent on the IO thread when the system should try to reduce the amount of
  // memory in use, no source or details are passed. See memory_purger.h .cc.
  NOTIFICATION_PURGE_MEMORY,

  // Sent before the repost form warning is brought up.
  // The source is a NavigationController.
  NOTIFICATION_REPOST_WARNING_SHOWN,

  // Sent when the zoom level changes. The source is the HostZoomMap.  The
  // details is a string of the hostname for which the zoom changed.  In case
  // of a temporary zoom level change, the details is an empty string.
  NOTIFICATION_ZOOM_LEVEL_CHANGED,

  // Custom notifications used by the embedder should start from here.
  NOTIFICATION_CONTENT_END,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
