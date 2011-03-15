// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NOTIFICATION_TYPE_H_
#define CONTENT_COMMON_NOTIFICATION_TYPE_H_
#pragma once

// This file describes various types used to describe and filter notifications
// that pass through the NotificationService.
//
// It is written as an enum inside a class so that it can be forward declared.
// You're not allowed to forward declare an enum, and we want to forward
// declare this since it's required by NotificationObserver which is included
// by a lot of header files.
//
// Since this class encapsulates an integral value, it should be passed by
// value.
class NotificationType {
 public:
  enum Type {
    // General -----------------------------------------------------------------

    // Special signal value to represent an interest in all notifications.
    // Not valid when posting a notification.
    ALL = 0,

    // The app is done processing user actions, now is a good time to do
    // some background work.
    IDLE,

    // Means that the app has just started doing something in response to a
    // user action, and that background processes shouldn't run if avoidable.
    BUSY,

    // This is sent when the user does a gesture resulting in a noteworthy
    // action taking place. This is typically used for logging. The source is
    // the profile, and the details is a string identifying the action.
    USER_ACTION,

    // NavigationController ----------------------------------------------------

    // A new pending navigation has been created. Pending entries are created
    // when the user requests the navigation. We don't know if it will actually
    // happen until it does (at this point, it will be "committed." Note that
    // renderer- initiated navigations such as link clicks will never be
    // pending.
    //
    // This notification is called after the pending entry is created, but
    // before we actually try to navigate. The source will be the
    // NavigationController that owns the pending entry, and there are no
    // details.
    NAV_ENTRY_PENDING,

    // A new non-pending navigation entry has been created. This will
    // correspond to one NavigationController entry being created (in the case
    // of new navigations) or renavigated to (for back/forward navigations).
    //
    // The source will be the navigation controller doing the commit. The
    // details will be NavigationController::LoadCommittedDetails.
    NAV_ENTRY_COMMITTED,

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
    NAV_LIST_PRUNED,

    // Indicates that a NavigationEntry has changed. The source will be the
    // NavigationController that owns the NavigationEntry. The details will be
    // a NavigationController::EntryChangedDetails struct.
    //
    // This will NOT be sent on navigation, interested parties should also
    // listen for NAV_ENTRY_COMMITTED to handle that case. This will be
    // sent when the entry is updated outside of navigation (like when a new
    // title comes).
    NAV_ENTRY_CHANGED,

    // Other load-related (not from NavigationController) ----------------------

    // Corresponds to ViewHostMsg_DocumentOnLoadCompletedInMainFrame. The source
    // is the TabContents and the details the page_id.
    LOAD_COMPLETED_MAIN_FRAME,

    // A content load is starting.  The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // is occurring.  No details are expected for this notification.
    LOAD_START,

    // A content load has stopped. The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // is occurring.  Details in the form of a LoadNotificationDetails object
    // are optional.
    LOAD_STOP,

    // A frame is staring a provisional load.  The source is a
    // Source<NavigationController> corresponding to the tab in which the load
    // occurs.  Details is a ProvisionalLoadDetails object.
    FRAME_PROVISIONAL_LOAD_START,

    // The provisional load for a frame was committed. The source is a
    // NavigationController corresponding to the tab in which the load occurred.
    // Details is a ProvisionalLoadDetails object. In contrast to
    // NAV_ENTRY_COMMITTED, this notification is sent when the load was
    // committed, even if no navigation entry was committed (such as
    // AUTO_SUBFRAME navigations).
    FRAME_PROVISIONAL_LOAD_COMMITTED,

    // The DOM for a frame was fully constructed, but referenced resources
    // might not be fully loaded yet. The source is a
    // Source<NavigationController> corresponding to the tab in which the load
    // occurred. Details are the int64 frame ID.
    FRAME_DOM_CONTENT_LOADED,

    // The frame finished loading. The source is a Source<NavigationController>
    // corresponding to the tab in which the load occurred. Details are the
    // int64 frame ID.
    FRAME_DID_FINISH_LOAD,

    // Content was loaded from an in-memory cache.  The source will be a
    // Source<NavigationController> corresponding to the tab in which the load
    // occurred.  Details in the form of a LoadFromMemoryCacheDetails object
    // are provided.
    LOAD_FROM_MEMORY_CACHE,

    // A provisional content load has failed with an error.  The source will be
    // a Source<NavigationController> corresponding to the tab in which the
    // load occurred.  Details in the form of a ProvisionalLoadDetails object
    // are provided.
    FAIL_PROVISIONAL_LOAD_WITH_ERROR,

    // A response has been received for a resource request.  The source will be
    // a Source<RenderViewHostDelegate> corresponding to the tab in which the
    // request was issued.  Details in the form of a ResourceRequestDetails
    // object are provided.
    RESOURCE_RESPONSE_STARTED,

    // A redirect was received while requesting a resource.  The source will be
    // a Source<RenderViewHostDelegate> corresponding to the tab in which the
    // request was issued.  Details in the form of a ResourceRedirectDetails
    // are provided.
    RESOURCE_RECEIVED_REDIRECT,

    // A new window is created in response to a request from a renderer. The
    // source will be a Source<TabContents> corresponding to the tab the
    // request originates from.  Details in the form of a
    // ViewHostMsg_CreateWindow_Params object are provided.
    CREATING_NEW_WINDOW,

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
    SSL_VISIBLE_STATE_CHANGED,

    // The SSL state of the browser has changed in some internal way.  For
    // example, the user might have explicitly allowed some broken certificate
    // or a secure origin might have included some insecure content.  Listen to
    // this notifiation if you need to keep track of our internal SSL state.
    //
    // The source will be the navigation controller associated with the state
    // change.  There are no details.
    SSL_INTERNAL_STATE_CHANGED,

    // Views -------------------------------------------------------------------

    // Notification that a view was removed from a view hierarchy.  The source
    // is the view, the details is the parent view.
    VIEW_REMOVED,

    // Browser-window ----------------------------------------------------------

    // This message is sent after a window has been opened.  The source is a
    // Source<Browser> containing the affected Browser.  No details are
    // expected.
    BROWSER_OPENED,

    // This message is sent soon after BROWSER_OPENED, and indicates that
    // the Browser's |window_| is now non-NULL. The source is a Source<Browser>
    // containing the affected Browser.  No details are expected.
    BROWSER_WINDOW_READY,

    // This message is sent when a browser is closing. The source is a
    // Source<Browser> containing the affected Browser. Details is a boolean
    // that if true indicates that the application will be closed as a result of
    // this browser window closure (i.e. this was the last opened browser
    // window on win/linux). This is sent prior to BROWSER_CLOSED, and may be
    // sent more than once for a particular browser.
    BROWSER_CLOSING,

    // This message is sent after a window has been closed.  The source is a
    // Source<Browser> containing the affected Browser.  Details is a boolean
    // that if true indicates that the last browser window has closed - this
    // does not indicate that the application is exiting (observers should
    // listen for APP_TERMINATING if they want to detect when the application
    // will shut down). Note that the boolean pointed to by details is only
    // valid for the duration of this call.
    BROWSER_CLOSED,

    // This message is sent when the last window considered to be an
    // "application window" has been closed. Dependent/dialog/utility windows
    // can use this as a way to know that they should also close. No source or
    // details are passed.
    ALL_APPWINDOWS_CLOSED,

#if defined(OS_MACOSX)
    // This message is sent when the application is made active (Mac OS X only
    // at present). No source or details are passed.
    APP_ACTIVATED,
#endif

    // This message is sent when the application is terminating (the last
    // browser window has shutdown as part of an explicit user-initiated exit,
    // or the user closed the last browser window on Windows/Linux and there are
    // no BackgroundContents keeping the browser running). No source or details
    // are passed.
    APP_TERMINATING,

#if defined(OS_MACOSX)
    // This notification is sent when the app has no key window, such as when
    // all windows are closed but the app is still active. No source or details
    // are provided.
    NO_KEY_WINDOW,
#endif

    // This is sent when the user has chosen to exit the app, but before any
    // browsers have closed. This is only sent if the user chooses the exit menu
    // item, not if Chrome exists by some other means (such as the user closing
    // the last window). The source and details are unspecified.
    APP_EXITING,

    // Indicates that a top window has been closed.  The source is the HWND
    // that was closed, no details are expected.
    WINDOW_CLOSED,

    // Indicates that a devtools window is closing. The source is the Profile*
    // and the details is the inspected RenderViewHost*.
    DEVTOOLS_WINDOW_CLOSING,

    // Sent when an info bubble has been created but not yet shown. The source
    // is the InfoBubble.
    INFO_BUBBLE_CREATED,

    // Sent when the language (English, French...) for a page has been detected.
    // The details Details<std::string> contain the ISO 639-1 language code and
    // the source is Source<TabContents>.
    TAB_LANGUAGE_DETERMINED,

    // Sent when a page has been translated. The source is the tab for that page
    // (Source<TabContents>) and the details are the language the page was
    // originally in and the language it was translated to
    // (std::pair<std::string, std::string>).
    PAGE_TRANSLATED,

    // Sent after the renderer returns a snapshot of tab contents.
    // The source (Source<RenderViewHost>) is the RenderViewHost for which the
    // snapshot was generated and the details (Details<const SkBitmap>) is the
    // actual snapshot.
    TAB_SNAPSHOT_TAKEN,

    // Send after the code is run in specified tab.
    TAB_CODE_EXECUTED,

    // The user has changed the browser theme.
    BROWSER_THEME_CHANGED,

    // Sent when the renderer returns focus to the browser, as part of focus
    // traversal. The source is the browser, there are no details.
    FOCUS_RETURNED_TO_BROWSER,

    // Application-modal dialogs -----------------------------------------------

    // Sent after an application-modal dialog has been shown. The source
    // is the dialog.
    APP_MODAL_DIALOG_SHOWN,

    // Tabs --------------------------------------------------------------------

    // Sent when a tab is added to a TabContentsDelegate. The source is the
    // TabContentsDelegate and the details is the TabContents.
    TAB_ADDED,

    // This notification is sent after a tab has been appended to the
    // tab_strip.  The source is a Source<NavigationController> with a pointer
    // to controller for the added tab. There are no details.
    TAB_PARENTED,

    // This message is sent before a tab has been closed.  The source is a
    // Source<NavigationController> with a pointer to the controller for the
    // closed tab.  No details are expected.
    //
    // See also TAB_CLOSED.
    TAB_CLOSING,

    // Notification that a tab has been closed. The source is the
    // NavigationController with no details.
    TAB_CLOSED,

    // This notification is sent when a render view host has connected to a
    // renderer process. The source is a Source<TabContents> with a pointer to
    // the TabContents.  A TAB_CONTENTS_DISCONNECTED notification is
    // guaranteed before the source pointer becomes junk.  No details are
    // expected.
    TAB_CONTENTS_CONNECTED,

    // This notification is sent when a TabContents swaps its render view host
    // with another one, possibly changing processes. The source is a
    // Source<TabContents> with a pointer to the TabContents.  A
    // TAB_CONTENTS_DISCONNECTED notification is guaranteed before the
    // source pointer becomes junk.  No details are expected.
    TAB_CONTENTS_SWAPPED,

    // This message is sent after a TabContents is disconnected from the
    // renderer process.  The source is a Source<TabContents> with a pointer to
    // the TabContents (the pointer is usable).  No details are expected.
    TAB_CONTENTS_DISCONNECTED,

    // This notification is sent after TabContents' title is updated. The source
    // is a Source<TabContents> with a pointer to the TabContents. No details
    // are expected.
    TAB_CONTENTS_TITLE_UPDATED,

    // This message is sent when a new InfoBar has been added to a TabContents.
    // The source is a Source<TabContents> with a pointer to the TabContents
    // the InfoBar was added to. The details is a Details<InfoBarDelegate> with
    // a pointer to an object implementing the InfoBarDelegate interface for
    // the InfoBar that was added.
    TAB_CONTENTS_INFOBAR_ADDED,

    // This message is sent when an InfoBar is about to be removed from a
    // TabContents. The source is a Source<TabContents> with a pointer to the
    // TabContents the InfoBar was removed from. The details is a
    // Details<InfoBarDelegate> with a pointer to an object implementing the
    // InfoBarDelegate interface for the InfoBar that was removed.
    TAB_CONTENTS_INFOBAR_REMOVED,

    // This message is sent when an InfoBar is replacing another infobar in a
    // TabContents. The source is a Source<TabContents> with a pointer to the
    // TabContents the InfoBar was removed from. The details is a
    // Details<std::pair<InfoBarDelegate*, InfoBarDelegate*> > with a pointer
    // to the old and new InfoBarDelegates, respectively.
    TAB_CONTENTS_INFOBAR_REPLACED,

    // This is sent when an externally hosted tab is created. The details
    // contain the ExternalTabContainer that contains the tab
    EXTERNAL_TAB_CREATED,

    // This is sent when an externally hosted tab is closed.  No details are
    // expected.
    EXTERNAL_TAB_CLOSED,

    // Indicates that the new page tab has finished loading. This is used for
    // performance testing to see how fast we can load it after startup, and is
    // only called once for the lifetime of the browser. The source is unused.
    // Details is an integer: the number of milliseconds elapsed between
    // starting and finishing all painting.
    INITIAL_NEW_TAB_UI_LOAD,

    // Used to fire notifications about how long various events took to
    // complete.  E.g., this is used to get more fine grained timings from the
    // new tab page.  Details is a MetricEventDurationDetails.
    METRIC_EVENT_DURATION,

    // This notification is sent when a TabContents is being hidden, e.g. due
    // to switching away from this tab.  The source is a Source<TabContents>.
    TAB_CONTENTS_HIDDEN,

    // This notification is sent when a TabContents is being destroyed. Any
    // object holding a reference to a TabContents can listen to that
    // notification to properly reset the reference. The source is a
    // Source<TabContents>.
    TAB_CONTENTS_DESTROYED,

    // This notification is sent when TabContents::SetAppExtension is invoked.
    // The source is the TabContents SetAppExtension was invoked on.
    TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,

    // A RenderViewHost was created for a TabContents. The source is the
    // associated TabContents, and the details is the RenderViewHost
    // pointer.
    RENDER_VIEW_HOST_CREATED_FOR_TAB,

    // Notification than an interstitial has become associated with a tab. The
    // source is the TabContents, the details not used.
    INTERSTITIAL_ATTACHED,

    // Stuff inside the tabs ---------------------------------------------------

    // This message is sent after a constrained window has been closed.  The
    // source is a Source<ConstrainedWindow> with a pointer to the closed child
    // window.  (The pointer isn't usable, except for identification.) No
    // details are expected.
    CWINDOW_CLOSED,

    // Indicates that a RenderProcessHost was created and its handle is now
    // available. The source will be the RenderProcessHost that corresponds to
    // the process.
    RENDERER_PROCESS_CREATED,

    // Indicates that a RenderProcessHost is destructing. The source will be the
    // RenderProcessHost that corresponds to the process.
    RENDERER_PROCESS_TERMINATED,

    // Indicates that a render process was closed (meaning it exited, but the
    // RenderProcessHost might be reused).  The source will be the corresponding
    // RenderProcessHost.  The details will be a RendererClosedDetails struct.
    // This may get sent along with RENDERER_PROCESS_TERMINATED.
    RENDERER_PROCESS_CLOSED,

    // Indicates that a render process has become unresponsive for a period of
    // time. The source will be the RenderWidgetHost that corresponds to the
    // hung view, and no details are expected.
    RENDERER_PROCESS_HANG,

    // This is sent to notify that the RenderViewHost displayed in a
    // TabContents has changed.  Source is the TabContents for which the change
    // happened, details is the previous RenderViewHost (can be NULL when the
    // first RenderViewHost is set).
    RENDER_VIEW_HOST_CHANGED,

    // Indicates that the render view host has received an accessibility tree
    // update, either partial or full, from the render view.  The source is the
    // RenderViewHost, the details are not used.
    RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,

    // This is sent when a RenderWidgetHost is being destroyed. The source is
    // the RenderWidgetHost, the details are not used.
    RENDER_WIDGET_HOST_DESTROYED,

    // Sent when the widget is about to paint. The source is the
    // RenderWidgetHost, the details are not used.
    RENDER_WIDGET_HOST_WILL_PAINT,

    // Sent after the widget has painted. The source is the RenderWidgetHost,
    // the details are not used.
    RENDER_WIDGET_HOST_DID_PAINT,

    // Indicates the RenderWidgetHost is about to destroy the backing store. The
    // backing store will still be valid when this call is made. The source is
    // the RenderWidgetHost, the details is the BackingStore.
    RENDER_WIDGET_HOST_WILL_DESTROY_BACKING_STORE,

    // Indicates that the RenderWidgetHost just updated the backing store. The
    // source is the RenderWidgetHost, the details are not used.
    RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,

    // This notifies the observer that a PaintAtSizeACK was received. The source
    // is the RenderWidgetHost, the details are an instance of
    // RenderWidgetHost::PaintAtSizeAckDetails.
    RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,

    // This notifies the observer that a HandleInputEventACK was received. The
    // source is the RenderWidgetHost, the details are the type of event
    // received.
    // Note: The RenderWidgetHost may be deallocated at this point.
    // Used only in testing.
    RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,

    // Sent from ~RenderViewHost. The source is the TabContents.
    RENDER_VIEW_HOST_DELETED,

    // Sent from RenderViewHost::ClosePage.  The hosted RenderView has
    // processed the onbeforeunload handler and is about to be sent a
    // ViewMsg_ClosePage message to complete the tear-down process.  The source
    // is the RenderViewHost sending the message, and no details are provided.
    // Note:  This message is not sent in response to RenderView closure
    // initiated by window.close().
    RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,

    // Indicates a RenderWidgetHost has been hidden or restored. The source is
    // the RWH whose visibility changed, the details is a bool set to true if
    // the new state is "visible."
    RENDER_WIDGET_VISIBILITY_CHANGED,

    // Notification from TabContents that we have received a response from the
    // renderer in response to a dom automation controller action.
    DOM_OPERATION_RESPONSE,

    // Sent when the bookmark bubble hides. The source is the profile, the
    // details unused.
    BOOKMARK_BUBBLE_HIDDEN,

    // This notification is sent when the result of a find-in-page search is
    // available with the browser process. The source is a Source<TabContents>
    // with a pointer to the TabContents. Details encompass a
    // FindNotificationDetail object that tells whether the match was found or
    // not found.
    FIND_RESULT_AVAILABLE,

    // This is sent when the users preference for when the bookmark bar should
    // be shown changes. The source is the profile, and the details are
    // NoDetails.
    BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,

    // Sent just before the installation confirm dialog is shown. The source
    // is the ExtensionInstallUI, the details are NoDetails.
    EXTENSION_WILL_SHOW_CONFIRM_DIALOG,

    // Used to monitor web cache usage by notifying whenever the
    // CacheManagerHost observes new UsageStats. The source will be the
    // RenderProcessHost that corresponds to the new statistics. Details are a
    // UsageStats object sent by the renderer, and should be copied - ptr not
    // guaranteed to be valid after the notification.
    WEB_CACHE_STATS_OBSERVED,

    // The focused element inside a page has changed.  The source is the
    // TabContents containing the render view host for the page. The details is
    // a Details<const bool> that indicates whether or not an editable node was
    // focused.
    FOCUS_CHANGED_IN_PAGE,

    // Notification posted from ExecuteJavascriptInWebFrameNotifyResult. The
    // source is the RenderViewHost ExecuteJavascriptInWebFrameNotifyResult was
    // invoked on. The details are a std::pair<int, Value*> with the int giving
    // the id returned from ExecuteJavascriptInWebFrameNotifyResult and the
    // Value the results of the javascript expression. The Value is owned by
    // RenderViewHost and may be a Null Value.
    EXECUTE_JAVASCRIPT_RESULT,

    // BackgroundContents ------------------------------------------------------

    // A new background contents was opened by script. The source is the parent
    // profile and the details are BackgroundContentsOpenedDetails.
    BACKGROUND_CONTENTS_OPENED,

    // The background contents navigated to a new location. The source is the
    // parent Profile, and the details are the BackgroundContents that was
    // navigated.
    BACKGROUND_CONTENTS_NAVIGATED,

    // The background contents were closed by someone invoking window.close()
    // or the parent application was uninstalled.
    // The source is the parent profile, and the details are the
    // BackgroundContents.
    BACKGROUND_CONTENTS_CLOSED,

    // The background contents is being deleted. The source is the
    // parent Profile, and the details are the BackgroundContents being deleted.
    BACKGROUND_CONTENTS_DELETED,

    // Child Processes ---------------------------------------------------------

    // This notification is sent when a child process host has connected to a
    // child process.  There is no usable source, since it is sent from an
    // ephemeral task; register for AllSources() to receive this notification.
    // The details are in a Details<ChildProcessInfo>.
    CHILD_PROCESS_HOST_CONNECTED,

    // This message is sent after a ChildProcessHost is disconnected from the
    // child process.  There is no usable source, since it is sent from an
    // ephemeral task; register for AllSources() to receive this notification.
    // The details are in a Details<ChildProcessInfo>.
    CHILD_PROCESS_HOST_DISCONNECTED,

    // This message is sent when a child process disappears
    // unexpectedly as a result of a crash.  There is no usable
    // source, since it is sent from an ephemeral task; register for
    // AllSources() to receive this notification.  The details are in
    // a Details<ChildProcessInfo>.
    CHILD_PROCESS_CRASHED,

    // This message is sent when a child process disappears
    // unexpectedly as a result of a termination signal.  There is no
    // usable source, since it is sent from an ephemeral task;
    // register for AllSources() to receive this notification.  The
    // details are in a Details<ChildProcessInfo>.
    CHILD_PROCESS_WAS_KILLED,

    // This message indicates that an instance of a particular child was
    // created in a page.  (If one page contains several regions rendered by
    // the same child, this notification will occur once for each region
    // during the page load.)
    //
    // There is no usable source, since it is sent from an ephemeral task;
    // register for AllSources() to receive this notification.  The details are
    // in a Details<ChildProcessInfo>.
    CHILD_INSTANCE_CREATED,

    // This is sent when network interception is disabled for a plugin, or the
    // plugin is unloaded.  This should only be sent/received on the browser IO
    // thread or the plugin thread. The source is the plugin that is disabling
    // interception.  No details are expected.
    CHROME_PLUGIN_UNLOADED,

    // Sent by the PluginUpdater when there is a change of plugin
    // enable/disable status.
    PLUGIN_ENABLE_STATUS_CHANGED,

    // This is sent when a login prompt is shown.  The source is the
    // Source<NavigationController> for the tab in which the prompt is shown.
    // Details are a LoginNotificationDetails which provide the LoginHandler
    // that should be given authentication.
    AUTH_NEEDED,

    // This is sent when authentication credentials have been supplied (either
    // by the user or by an automation service), but before we've actually
    // received another response from the server.  The source is the
    // Source<NavigationController> for the tab in which the prompt was shown.
    // Details are an AuthSuppliedLoginNotificationDetails which provide the
    // LoginHandler that should be given authentication as well as the supplied
    // username and password.
    AUTH_SUPPLIED,

    // This is sent when an authentication request has been dismissed without
    // supplying credentials (either by the user or by an automation service).
    // The source is the Source<NavigationController> for the tab in which the
    // prompt was shown. Details are a LoginNotificationDetails which provide
    // the LoginHandler that should be cancelled.
    AUTH_CANCELLED,

    // Saved Pages -------------------------------------------------------------

    // Sent when a SavePackage finishes successfully. The source is the
    // SavePackage, and Details are a GURL containing address of downloaded
    // page.
    SAVE_PACKAGE_SUCCESSFULLY_FINISHED,

    // History -----------------------------------------------------------------

    // Sent when a history service is created on the main thread. This is sent
    // after history is created, but before it has finished loading. Use
    // HISTORY_LOADED is you need to know when loading has completed.
    // The source is the profile that the history service belongs to, and the
    // details is the pointer to the newly created HistoryService object.
    HISTORY_CREATED,

    // Sent when a history service has finished loading. The source is the
    // profile that the history service belongs to, and the details is the
    // HistoryService.
    HISTORY_LOADED,

    // Sent when a URL that has been typed has been added or modified. This is
    // used by the in-memory URL database (used by autocomplete) to track
    // changes to the main history system.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLsModifiedDetails that lists the modified or
    // added URLs.
    HISTORY_TYPED_URLS_MODIFIED,

    // Sent when the user visits a URL.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLVisitedDetails.
    HISTORY_URL_VISITED,

    // Sent when one or more URLs are deleted.
    //
    // The source is the profile owning the history service that changed, and
    // the details is history::URLsDeletedDetails that lists the deleted URLs.
    HISTORY_URLS_DELETED,

    // Sent when a keyword search term is updated. The source is the Profile and
    // the details are history::KeywordSearchTermDetails
    HISTORY_KEYWORD_SEARCH_TERM_UPDATED,

    // Sent by history when the favicon of a URL changes.  The source is the
    // profile, and the details is history::FaviconChangeDetails (see
    // history_notifications.h).
    FAVICON_CHANGED,

    // Sent by history if there is a problem reading the profile.  The details
    // is an int that's one of the message IDs in the string table.  The active
    // browser window should notify the user of this error.
    PROFILE_ERROR,

    // Sent after an incognito profile has been created. The details are none
    // and the source is the new profile.
    OTR_PROFILE_CREATED,

    // Sent before a Profile is destroyed. The details are
    // none and the source is a Profile*.
    PROFILE_DESTROYED,

    // TopSites ----------------------------------------------------------------

    // Sent by TopSites when it finishes loading. The source is the profile the
    // details the TopSites.
    TOP_SITES_LOADED,

    // Sent by TopSites when it has finished updating its most visited URLs
    // cache after querying the history service. The source is the TopSites and
    // the details a CancelableRequestProvider::Handle from the history service
    // query.
    // Used only in testing.
    TOP_SITES_UPDATED,

    // Thumbnails---------------------------------------------------------------

    // Sent by the ThumbnailGenerator whenever a render widget host
    // updates its backing store.  The source is the
    // ThumbnailGenerator, and the details are the RenderWidgetHost
    // that notified the ThumbnailGenerator that its backing store was
    // updated.
    THUMBNAIL_GENERATOR_SNAPSHOT_CHANGED,

    // Bookmarks ---------------------------------------------------------------

    // Sent when the starred state of a URL changes. A URL is starred if there
    // is at least one bookmark for it. The source is a Profile and the details
    // is history::URLsStarredDetails that contains the list of URLs and
    // whether they were starred or unstarred.
    URLS_STARRED,

    // Sent when the bookmark bar model finishes loading. This source is the
    // Profile, and the details aren't used.
    BOOKMARK_MODEL_LOADED,

    // Sent when SpellCheckHost has been reloaded. The source is the profile,
    // the details are NoDetails.
    SPELLCHECK_HOST_REINITIALIZED,

    // Sent when a new word has been added to the custom dictionary. The source
    // is the SpellCheckHost, the details are NoDetails.
    SPELLCHECK_WORD_ADDED,

    // Sent by the profile when the automatic spell correction setting has been
    // toggled. It exists as a notification rather than just letting interested
    // parties listen for the pref change because some objects may outlive the
    // profile. Source is profile, details is NoDetails.
    SPELLCHECK_AUTOSPELL_TOGGLED,

    // Sent when the bookmark bubble is shown for a particular URL. The source
    // is the profile, the details the URL.
    BOOKMARK_BUBBLE_SHOWN,

    // Non-history storage services --------------------------------------------

    // Notification that the TemplateURLModel has finished loading from the
    // database. The source is the TemplateURLModel, and the details are
    // NoDetails.
    TEMPLATE_URL_MODEL_LOADED,

    // Sent when a TemplateURL is removed from the model. The source is the
    // Profile, and the details the id of the TemplateURL being removed.
    TEMPLATE_URL_REMOVED,

    // Notification triggered when a web application has been installed or
    // uninstalled. Any application view should reload its data.  The source is
    // the profile. No details are provided.
    WEB_APP_INSTALL_CHANGED,

    // This is sent to a pref observer when a pref is changed. The source is the
    // PrefService and the details a std::string of the changed path.
    PREF_CHANGED,

    // This is broadcast after the preference subsystem has completed
    // asynchronous initalization of a PrefService.
    PREF_INITIALIZATION_COMPLETED,

    // Sent when a default request context has been created, so calling
    // Profile::GetDefaultRequestContext() will not return NULL.  This is sent
    // on the thread where Profile::GetRequestContext() is first called, which
    // should be the UI thread.
    DEFAULT_REQUEST_CONTEXT_AVAILABLE,

    // The state of a web resource has been changed. A resource may have been
    // added, removed, or altered. Source is WebResourceService, and the
    // details are NoDetails.
    PROMO_RESOURCE_STATE_CHANGED,

    // Autocomplete ------------------------------------------------------------

    // Sent by the autocomplete controller when done.  The source is the
    // AutocompleteController, the details not used.
    AUTOCOMPLETE_CONTROLLER_RESULT_READY,

    // This is sent when an item of the Omnibox popup is selected. The source
    // is the profile.
    OMNIBOX_OPENED_URL,

    // Sent by the autocomplete edit when it is destroyed.
    AUTOCOMPLETE_EDIT_DESTROYED,

    // Sent by the autocomplete edit when it is focused.
    AUTOCOMPLETE_EDIT_FOCUSED,

    // Sent when the main Google URL has been updated.  Some services cache
    // this value and need to update themselves when it changes.  See
    // google_util::GetGoogleURLAndUpdateIfNecessary().
    GOOGLE_URL_UPDATED,

    // Printing ----------------------------------------------------------------

    // Notification from PrintJob that an event occured. It can be that a page
    // finished printing or that the print job failed. Details is
    // PrintJob::EventDetails.
    PRINT_JOB_EVENT,

    // Shutdown ----------------------------------------------------------------

    // Sent on the browser IO thread when an net::URLRequestContext is released
    // by its owning Profile.  The source is a pointer to the
    // net::URLRequestContext.
    URL_REQUEST_CONTEXT_RELEASED,

    // Sent when WM_ENDSESSION has been received, after the browsers have been
    // closed but before browser process has been shutdown. The source/details
    // are all source and no details.
    SESSION_END,

    // Personalization ---------------------------------------------------------

    PERSONALIZATION,
    PERSONALIZATION_CREATED,

    // User Scripts ------------------------------------------------------------

    // Sent when there are new user scripts available.  The details are a
    // pointer to SharedMemory containing the new scripts.
    USER_SCRIPTS_UPDATED,

    // User Style Sheet --------------------------------------------------------

    // Sent when the user style sheet has changed.
    USER_STYLE_SHEET_UPDATED,

    // Extensions --------------------------------------------------------------

    // Sent when the known installed extensions have all been loaded.  In
    // testing scenarios this can happen multiple times if extensions are
    // unloaded and reloaded. The source is a Profile.
    EXTENSIONS_READY,

    // Sent when a new extension is loaded. The details are an Extension, and
    // the source is a Profile.
    EXTENSION_LOADED,

    // Sent when attempting to load a new extension, but they are disabled. The
    // details are an Extension*, and the source is a Profile*.
    EXTENSION_UPDATE_DISABLED,

    // Sent when an extension is about to be installed so we can (in the case of
    // themes) alert the user with a loading dialog. The source is the download
    // manager and the details are the download url.
    EXTENSION_READY_FOR_INSTALL,

    // Sent when an extension install turns out to not be a theme.
    NO_THEME_DETECTED,

    // Sent when a new theme is installed. The details are an Extension, and the
    // source is a Profile.
    THEME_INSTALLED,

    // Sent when new extensions are installed. The details are an Extension, and
    // the source is a Profile.
    EXTENSION_INSTALLED,

    // An error occured during extension install. The details are a string with
    // details about why the install failed.
    EXTENSION_INSTALL_ERROR,

    // Sent when an extension has been uninstalled.  The details are
    // an UninstalledExtensionInfo struct and the source is a Profile.
    EXTENSION_UNINSTALLED,

    // Sent when an extension is unloaded. This happens when an extension is
    // uninstalled or disabled. The details are an UnloadedExtensionInfo, and
    // the source is a Profile.
    //
    // Note that when this notification is sent, ExtensionService has already
    // removed the extension from its internal state.
    EXTENSION_UNLOADED,

    // Sent when an extension has updated its user scripts. The details are an
    // Extension, and the source is a Profile.
    EXTENSION_USER_SCRIPTS_UPDATED,

    // Sent after a new ExtensionFunctionDispatcher is created. The details are
    // an ExtensionFunctionDispatcher* and the source is a Profile*. This is
    // similar in timing to EXTENSION_HOST_CREATED, but also fires when an
    // extension view which is hosted in TabContents* is created.
    EXTENSION_FUNCTION_DISPATCHER_CREATED,

    // Sent before an ExtensionHost is destroyed. The details are
    // an ExtensionFunctionDispatcher* and the source is a Profile*. This is
    // similar in timing to EXTENSION_HOST_DESTROYED, but also fires when an
    // extension view which is hosted in TabContents* is destroyed.
    EXTENSION_FUNCTION_DISPATCHER_DESTROYED,

    // Sent after a new ExtensionHost is created. The details are
    // an ExtensionHost* and the source is an ExtensionProcessManager*.
    EXTENSION_HOST_CREATED,

    // Sent before an ExtensionHost is destroyed. The details are
    // an ExtensionHost* and the source is a Profile*.
    EXTENSION_HOST_DESTROYED,

    // Sent by an ExtensionHost when it finished its initial page load.
    // The details are an ExtensionHost* and the source is a Profile*.
    EXTENSION_HOST_DID_STOP_LOADING,

    // Sent by an ExtensionHost when its render view requests closing through
    // window.close(). The details are an ExtensionHost* and the source is a
    // Profile*.
    EXTENSION_HOST_VIEW_SHOULD_CLOSE,

    // Sent after an extension render process is created and fully functional.
    // The details are an ExtensionHost*.
    EXTENSION_PROCESS_CREATED,

    // Sent when extension render process ends (whether it crashes or closes).
    // The details are an ExtensionHost* and the source is a Profile*. Not sent
    // during browser shutdown.
    EXTENSION_PROCESS_TERMINATED,

    // Sent when a background page is ready so other components can load.
    EXTENSION_BACKGROUND_PAGE_READY,

    // Sent when a pop-up extension view is ready, so that notification may
    // be sent to pending callbacks.  Note that this notification is sent
    // after all onload callbacks have been invoked in the main frame.
    // The details is the ExtensionHost* hosted within the popup, and the source
    // is a Profile*.
    EXTENSION_POPUP_VIEW_READY,

    // Sent when a browser action's state has changed. The source is the
    // ExtensionAction* that changed.  There are no details.
    EXTENSION_BROWSER_ACTION_UPDATED,

    // Sent when the count of page actions has changed. Note that some of them
    // may not apply to the current page. The source is a LocationBar*. There
    // are no details.
    EXTENSION_PAGE_ACTION_COUNT_CHANGED,

    // Sent when a browser action's visibility has changed. The source is the
    // ExtensionPrefs* that changed. The details are a Extension*.
    EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,

    // Sent when a page action's visibility has changed. The source is the
    // ExtensionAction* that changed. The details are a TabContents*.
    EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,

    // Sent by an extension to notify the browser about the results of a unit
    // test.
    EXTENSION_TEST_PASSED,
    EXTENSION_TEST_FAILED,

    // Sent by extension test javascript code, typically in a browser test. The
    // sender is a std::string representing the extension id, and the details
    // are a std::string with some message. This is particularly useful when you
    // want to have C++ code wait for javascript code to do something.
    EXTENSION_TEST_MESSAGE,

    // Sent when an bookmarks extensions API function was successfully invoked.
    // The source is the id of the extension that invoked the function, and the
    // details are a pointer to the const BookmarksFunction in question.
    EXTENSION_BOOKMARKS_API_INVOKED,

    // Sent when an omnibox extension has sent back omnibox suggestions. The
    // source is the profile, and the details are an ExtensionOmniboxSuggestions
    // object.
    EXTENSION_OMNIBOX_SUGGESTIONS_READY,

    // Sent when the user accepts the input in an extension omnibox keyword
    // session. The source is the profile.
    EXTENSION_OMNIBOX_INPUT_ENTERED,

    // Sent when an omnibox extension has updated the default suggestion. The
    // source is the profile.
    EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,

    // Sent when an extension changes a preference value. The source is the
    // profile, and the details are an ExtensionPrefStore::ExtensionPrefDetails
    // object.
    EXTENSION_PREF_CHANGED,

    // Desktop Notifications ---------------------------------------------------

    // This notification is sent when a balloon is connected to a renderer
    // process to render the balloon contents.  The source is a
    // Source<BalloonHost> with a pointer to the the balloon.  A
    // NOTIFY_BALLOON_DISCONNECTED is guaranteed before the source pointer
    // becomes junk. No details expected.
    NOTIFY_BALLOON_CONNECTED,

    // This message is sent after a balloon is disconnected from the renderer
    // process. The source is a Source<BalloonHost> with a pointer to the
    // balloon host (the pointer is usable). No details are expected.
    NOTIFY_BALLOON_DISCONNECTED,

    // Web Database Service ----------------------------------------------------

    // This notification is sent whenever autofill entries are
    // changed.  The detail of this notification is a list of changes
    // represented by a vector of AutofillChange.  Each change
    // includes a change type (add, update, or remove) as well as the
    // key of the entry that was affected.
    AUTOFILL_ENTRIES_CHANGED,

    // Sent when an AutofillProfile has been added/removed/updated in the
    // WebDatabase.  The detail is an AutofillProfileChange.
    AUTOFILL_PROFILE_CHANGED,

    // Sent when an Autofill CreditCard has been added/removed/updated in the
    // WebDatabase.  The detail is an AutofillCreditCardChange.
    AUTOFILL_CREDIT_CARD_CHANGED,

    // This notification is sent whenever the web database service has finished
    // loading the web database.  No details are expected.
    WEB_DATABASE_LOADED,

    // Purge Memory ------------------------------------------------------------

    // Sent on the IO thread when the system should try to reduce the amount of
    // memory in use, no source or details are passed. See memory_purger.h .cc.
    PURGE_MEMORY,

    // Upgrade notifications ---------------------------------------------------

    // Sent when Chrome detects that it has been upgraded behind the scenes.
    // NOTE: The detection mechanism is asynchronous, so this event may arrive
    // quite some time after the upgrade actually happened. No details are
    // expected.
    UPGRADE_DETECTED,

    // Sent when Chrome believes an update has been installed and available for
    // long enough with the user shutting down to let it take effect. See
    // upgrade_detector.cc for details on how long it waits. No details are
    // expected.
    UPGRADE_RECOMMENDED,

    // Software incompatibility notifications ----------------------------------

    // Sent when Chrome has finished compiling the list of loaded modules (and
    // other modules of interest). No details are expected.
    MODULE_LIST_ENUMERATED,

    // Sent when Chrome detects an incompatible module. Details is a boolean
    // specifying true if one or more confirmed bad modules were found or false
    // if only suspected bad modules were found.
    MODULE_INCOMPATIBILITY_DETECTED,

    // Background App Tracking Notifications -----------------------------------
    // Sent when the state of the background page tracker has changed (the
    // number of unacknowledged background pages have changed). Source is the
    // BackgroundPageTracker and there are no Details.
    BACKGROUND_PAGE_TRACKER_CHANGED,

    // Accessibility Notifications ---------------------------------------------

    // Notification that a window in the browser UI (not the web content)
    // was opened, for propagating to an accessibility extension.
    // Details will be an AccessibilityWindowInfo.
    ACCESSIBILITY_WINDOW_OPENED,

    // Notification that a window in the browser UI was closed.
    // Details will be an AccessibilityWindowInfo.
    ACCESSIBILITY_WINDOW_CLOSED,

    // Notification that a control in the browser UI was focused.
    // Details will be an AccessibilityControlInfo.
    ACCESSIBILITY_CONTROL_FOCUSED,

    // Notification that a control in the browser UI had its action taken,
    // like pressing a button or toggling a checkbox.
    // Details will be an AccessibilityControlInfo.
    ACCESSIBILITY_CONTROL_ACTION,

    // Notification that text box in the browser UI had text change.
    // Details will be an AccessibilityControlInfo.
    ACCESSIBILITY_TEXT_CHANGED,

    // Notification that a pop-down menu was opened, for propagating
    // to an accessibility extension.
    // Details will be an AccessibilityMenuInfo.
    ACCESSIBILITY_MENU_OPENED,

    // Notification that a pop-down menu was closed, for propagating
    // to an accessibility extension.
    // Details will be an AccessibilityMenuInfo.
    ACCESSIBILITY_MENU_CLOSED,

    // Content Settings --------------------------------------------------------

    // Sent when content settings change. The source is a HostContentSettings
    // object, the details are ContentSettingsNotificationsDetails.
    CONTENT_SETTINGS_CHANGED,

    // Sent when the collect cookies dialog is shown. The source is a
    // TabSpecificContentSettings object, there are no details.
    COLLECTED_COOKIES_SHOWN,

    // Sent when the default setting for desktop notifications has changed.
    // The source is the DesktopNotificationService, the details are None.
    DESKTOP_NOTIFICATION_DEFAULT_CHANGED,

    // Sent when a non-default setting in the the notification content settings
    // map has changed. The source is the DesktopNotificationService, the
    // details are None.
    DESKTOP_NOTIFICATION_SETTINGS_CHANGED,

    // Sent when the geolocation settings change. The source is the
    // GeolocationContentSettingsMap object, the details are
    // ContentSettingsNotificationsDetails.
    GEOLOCATION_SETTINGS_CHANGED,

    // Sync --------------------------------------------------------------------

    // Sent when the sync backend has been paused.
    SYNC_PAUSED,

    // Sent when the sync backend has been resumed.
    SYNC_RESUMED,

    // The sync service has started the configuration process.
    SYNC_CONFIGURE_START,

    // The sync service is finished the configuration process.
    SYNC_CONFIGURE_DONE,

    // The session service has been saved.  This notification type is only sent
    // if there were new SessionService commands to save, and not for no-op save
    // operations.
    SESSION_SERVICE_SAVED,

    // A foreign session has been updated.  If a new tab page is open, the
    // foreign session handler needs to update the new tab page's foreign
    // session data.
    FOREIGN_SESSION_UPDATED,

    // Foreign sessions has been disabled. New tabs should not display foreign
    // session data.
    FOREIGN_SESSION_DISABLED,

    // Sent when the set of data types that should be synced has been modified
    // externally (eg. by the WebUI options screen).
    // The source is the Profile, there are no details.
    SYNC_DATA_TYPES_UPDATED,

    // Cookies -----------------------------------------------------------------

    // Sent when a cookie changes. The source is a Profile object, the details
    // are a ChromeCookieDetails object.
    COOKIE_CHANGED,

    // Sidebar -----------------------------------------------------------------

    // Sent when the sidebar state is changed.
    // The source is a SidebarManager instance, the details are the changed
    // SidebarContainer object.
    SIDEBAR_CHANGED,

    // Token Service -----------------------------------------------------------

    // When the token service has a new token available for a service, one of
    // these notifications is issued per new token.
    // The source is a TokenService on the Profile. The details are a
    // TokenAvailableDetails object.
    TOKEN_AVAILABLE,

    // When there aren't any additional tokens left to load, this notification
    // is sent.
    // The source is a TokenService on the profile. There are no details.
    TOKEN_LOADING_FINISHED,

    // If a token request failed, one of these is issued per failed request.
    // The source is a TokenService on the Profile. The details are a
    // TokenRequestFailedDetails object.
    TOKEN_REQUEST_FAILED,

    // When a service has a new token they got from a frontend that the
    // TokenService should know about, fire this notification. The details
    // are a TokenAvailableDetails object.
    TOKEN_UPDATED,

    // Sent when a user signs into Google services such as sync.
    // The source is the Profile. The details are a GoogleServiceSignin object.
    GOOGLE_SIGNIN_SUCCESSFUL,

    // Sent when a user fails to sign into Google services such as sync.
    // The source is the Profile. The details are a GoogleServiceAuthError
    // object.
    GOOGLE_SIGNIN_FAILED,

    // AutoFill Notifications --------------------------------------------------

    // Sent when a popup with AutoFill suggestions is shown in the renderer.
    // The source is the corresponding RenderViewHost. There are not details.
    AUTOFILL_DID_SHOW_SUGGESTIONS,

    // Sent when a form is previewed or filled with AutoFill suggestions.
    // The source is the corresponding RenderViewHost. There are not details.
    AUTOFILL_DID_FILL_FORM_DATA,

    // Download Notifications --------------------------------------------------

    // Sent when a download is initiated. It is possible that the download will
    // not actually begin due to the DownloadRequestLimiter cancelling it
    // prematurely.
    // The source is the corresponding RenderViewHost. There are no details.
    DOWNLOAD_INITIATED,

    // Misc --------------------------------------------------------------------

#if defined(OS_CHROMEOS)
    // Sent when a chromium os user logs in.
    LOGIN_USER_CHANGED,

    // Sent when user image is updated.
    LOGIN_USER_IMAGE_CHANGED,

    // Sent when a chromium os user attempts to log in.  The source is
    // all and the details are AuthenticationNotificationDetails.
    LOGIN_AUTHENTICATION,

    // Sent when a panel state changed.
    PANEL_STATE_CHANGED,

    // Sent when the wizard's content view is destroyed. The source and details
    // are not used.
    WIZARD_CONTENT_VIEW_DESTROYED,

    // Sent when the screen lock state has changed. The source is
    // ScreenLocker and the details is a bool specifing that the
    // screen is locked. When details is a false, the source object
    // is being deleted, so the receiver shouldn't use the screen locker
    // object.
    SCREEN_LOCK_STATE_CHANGED,

    // Sent when the network state has changed on UI thread.
    // The source is AllSources and the details is NetworkStateDetails defined
    // in chrome/browser/chromeos/network_state_notifier.h.
    // TODO(oshima): Port this to all platforms.
    NETWORK_STATE_CHANGED,

    // Sent when an attempt to acquire the public key of the owner of a chromium
    // os device has succeeded.
    OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,

    // Sent when an attempt to acquire the public key of the owner of a chromium
    // os device has failed.
    OWNER_KEY_FETCH_ATTEMPT_FAILED,

    // Sent after device was successfully owned.
    OWNERSHIP_TAKEN,

    // This is sent to a ChromeOS settings observer when a system setting is
    // changed. The source is the CrosSettings and the details a std::string of
    // the changed setting.
    SYSTEM_SETTING_CHANGED,
#endif

    // Sent before the repost form warning is brought up.
    // The source is a NavigationController.
    REPOST_WARNING_SHOWN,

#if defined(TOOLKIT_VIEWS)
    // Sent when a bookmark's context menu is shown. Used to notify
    // tests that the context menu has been created and shown.
    BOOKMARK_CONTEXT_MENU_SHOWN,
#endif

    // Sent when the zoom level changes. The source is the profile.
    ZOOM_LEVEL_CHANGED,

    // Sent when the tab's closeable state has changed due to increase/decrease
    // in number of tabs in browser or increase/decrease in number of browsers.
    // Details<bool> contain the closeable flag while source is AllSources.
    // This is only sent from ChromeOS's TabCloseableStateWatcher.
    TAB_CLOSEABLE_STATE_CHANGED,

    // Sent each time the InstantController is updated.
    INSTANT_CONTROLLER_UPDATED,

    // Sent each time the InstantController shows the InstantLoader.
    INSTANT_CONTROLLER_SHOWN,

    // Password Store ----------------------------------------------------------
    // This notification is sent whenenever login entries stored in the password
    // store are changed. The detail of this notification is a list of changes
    // represented by a vector of PasswordStoreChange. Each change includes a
    // change type (ADD, UPDATE, or REMOVE) as well as the
    // |webkit_glue::PasswordForm|s that were affected.
    LOGINS_CHANGED,

    // Sent when the applications in the NTP app launcher have been reordered.
    EXTENSION_LAUNCHER_REORDERED,

    // Count (must be last) ----------------------------------------------------
    // Used to determine the number of notification types.  Not valid as
    // a type parameter when registering for or posting notifications.
    NOTIFICATION_TYPE_COUNT
  };

  // TODO(erg): Our notification system relies on implicit conversion.
  NotificationType(Type v) : value(v) {}  // NOLINT

  bool operator==(NotificationType t) const { return value == t.value; }
  bool operator!=(NotificationType t) const { return value != t.value; }

  // Comparison to explicit enum values.
  bool operator==(Type v) const { return value == v; }
  bool operator!=(Type v) const { return value != v; }

  Type value;
};

inline bool operator==(NotificationType::Type a, NotificationType b) {
  return a == b.value;
}
inline bool operator!=(NotificationType::Type a, NotificationType b) {
  return a != b.value;
}

#endif  // CONTENT_COMMON_NOTIFICATION_TYPE_H_
