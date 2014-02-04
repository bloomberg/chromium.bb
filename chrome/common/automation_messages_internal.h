// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the IPC messages used by the automation interface.

// NOTE: All IPC messages have either a routing_id of 0 (for asynchronous
//       messages), or one that's been assigned by the proxy (for calls
//       which expect a response).  The routing_id shouldn't be used for
//       any other purpose in these message types.

// NOTE: All the new IPC messages should go at the end.
//       The test <--> browser IPC message IDs need to match the reference
//       builds.  Since we now define the IDs based on __LINE__, to allow these
//       IPC messages to be used to control an old version of Chrome we need
//       the message IDs to remain the same.  This means that you should not
//       change the line number of these types of messages.


#define IPC_MESSAGE_START AutomationMsgStart

// This message is fired when the AutomationProvider is up and running
// in the app (the app is not fully up at this point). The parameter to this
// message is the version string of the automation provider. This parameter
// is defined to be the version string as returned by
// chrome::VersionInfo::Version().
// The client can choose to use this version string to decide whether or not
// it can talk to the provider.
IPC_MESSAGE_CONTROL1(AutomationMsg_Hello,
                     std::string)

// This message is fired when the initial tab(s) are finished loading.
IPC_MESSAGE_CONTROL0(AutomationMsg_InitialLoadsComplete)

// This message notifies the AutomationProvider to append a new tab the
// window with the given handle. The return value contains the index of
// the new tab, or -1 if the request failed.
// The second parameter is the url to be loaded in the new tab.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_AppendTab,
                            int,
                            GURL,
                            int)

// This message requests the (zero-based) index for the currently
// active tab in the window with the given handle. The return value contains
// the index of the active tab, or -1 if the request failed.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_ActiveTabIndex,
                            int,
                            int)

// This message notifies the AutomationProvider to active the tab.
// The first parameter is the handle to window resource.
// The second parameter is the (zero-based) index to be activated
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_ActivateTab,
                            int,
                            int,
                            int)

// This message requests the cookie value for given url in the
// profile of the tab identified by the second parameter.  The first
// parameter is the URL string. The response contains the length of the
// cookie value string. On failure, this length = -1.
IPC_SYNC_MESSAGE_CONTROL2_2(AutomationMsg_GetCookies,
                            GURL,
                            int,
                            int,
                            std::string)












// This message is used to implement the asynchronous version of
// NavigateToURL.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_NavigationAsync,
                            int /* tab handle */,
                            GURL,
                            bool /* result */)

// This message requests the number of browser windows that the app currently
// has open.  The return value is the number of windows.
IPC_SYNC_MESSAGE_CONTROL0_1(AutomationMsg_BrowserWindowCount,
                            int)

// This message requests the handle (int64 app-unique identifier) of the
// window with the given (zero-based) index.  On error, the returned handle
// value is 0.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_BrowserWindow,
                            int,
                            int)

// This message requests the number of tabs in the window with the given
// handle.  The return value contains the number of tabs, or -1 if the
// request failed.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_TabCount,
                            int,
                            int)

// This message requests the handle of the tab with the given (zero-based)
// index in the given app window. First parameter specifies the given window
// handle, second specifies the given tab_index. On error, the returned handle
// value is 0.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_Tab,
                            int,
                            int,
                            int)

// This message requests the the title of the tab with the given handle.
// The return value contains the size of the title string. On error, this
// value should be -1 and empty string. Note that the title can be empty in
// which case the size would be 0.
IPC_SYNC_MESSAGE_CONTROL1_2(AutomationMsg_TabTitle,
                            int,
                            int,
                            std::wstring)

// This message requests the url of the tab with the given handle.
// The return value contains a success flag and the URL string. The URL will
// be empty on failure, and it still may be empty on success.
IPC_SYNC_MESSAGE_CONTROL1_2(AutomationMsg_TabURL,
                            int /* tab handle */,
                            bool /* success flag */,
                            GURL)

// This message notifies the AutomationProxy that a handle that it has
// previously been given is now invalid.  (For instance, if the handle
// represented a window which has now been closed.)  The parameter
// value is the handle.
IPC_MESSAGE_CONTROL1(AutomationMsg_InvalidateHandle,
                     int)

// This message notifies the AutomationProvider that a handle is no
// longer being used, so it can stop paying attention to the
// associated resource.  The parameter value is the handle.
IPC_MESSAGE_CONTROL1(AutomationMsg_HandleUnused,
                     int)

// This message requests that the AutomationProvider executes a JavaScript,
// which is sent embedded in a 'javascript:' URL.
// The javascript is executed in context of child frame whose xpath
// is passed as parameter (context_frame). The execution results in
// a serialized JSON string response.
IPC_SYNC_MESSAGE_CONTROL3_1(AutomationMsg_DomOperation,
                            int /* tab handle */,
                            std::wstring /* context_frame */,
                            std::wstring /* the javascript to be executed */,
                            std::string /* the serialized json string containg
                                           the result of a javascript
                                           execution */)






// This message requests the bounds of the specified View element in
// window coordinates.
// Request:
//   int - the handle of the window in which the view appears
//   int - the ID of the view, as specified in chrome/browser/ui/view_ids.h
//   bool - whether the bounds should be returned in the screen coordinates
//          (if true) or in the browser coordinates (if false).
// Response:
//   bool - true if the view was found
//   gfx::Rect - the bounds of the view, in window coordinates
IPC_SYNC_MESSAGE_CONTROL3_2(AutomationMsg_WindowViewBounds,
                            int,
                            int,
                            bool,
                            bool,
                            gfx::Rect)

// This message sets the bounds of the window.
// Request:
//   int - the handle of the window to resize
//   gfx::Rect - the bounds of the window
// Response:
//   bool - true if the resize was successful
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_SetWindowBounds,
                            int,
                            gfx::Rect,
                            bool)




















// Similar to AutomationMsg_InitialLoadsComplete, this indicates that the
// new tab ui has completed the initial load of its data.
// Time is how many milliseconds the load took.
IPC_MESSAGE_CONTROL1(AutomationMsg_InitialNewTabUILoadComplete,
                    int /* time */)










// Opens a new browser window.
// TODO(sky): remove this and replace with OpenNewBrowserWindowOfType.
// Doing this requires updating the reference build.
IPC_SYNC_MESSAGE_CONTROL1_0(AutomationMsg_OpenNewBrowserWindow,
                            bool /* show */ )






// This message requests the window associated with the specified browser
// handle.
// The return value contains a success flag and the handle of the window.
IPC_SYNC_MESSAGE_CONTROL1_2(AutomationMsg_WindowForBrowser,
                            int /* browser handle */,
                            bool /* success flag */,
                            int /* window handle */)

























































// This message requests that a tab be closed.
// Request:
//   - int: handle of the tab to close
//   - bool: if true the proxy blocks until the tab has completely closed,
//           otherwise the proxy only blocks until it initiates the close.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_CloseTab,
                            int,
                            bool,
                            bool)

// This message requests that the browser be closed.
// Request:
//   - int: handle of the browser which contains the tab
// Response:
//  - bool: whether the operation was successfull.
//  - bool: whether the browser process will be terminated as a result (if
//          this was the last closed browser window).
IPC_SYNC_MESSAGE_CONTROL1_2(AutomationMsg_CloseBrowser,
                            int,
                            bool,
                            bool)


































































































































// Message to request that a browser window is brought to the front and
// activated.
// Request:
//   - int: handle of the browser window.
// Response:
//   - bool: True if the browser is brought to the front.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_BringBrowserToFront,
                            int,
                            bool)














// This message notifies the AutomationProvider to reload the current page in
// the tab with given handle. The first parameter is the handle to the tab
// resource.  The return value contains a status code which is nonnegative on
// success.
// see AutomationMsg_NavigationResponseValues for the navigation response.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_Reload,
                            int,
                            AutomationMsg_NavigationResponseValues)

// This message requests the execution of a browser command in the browser
// for which the handle is specified.
// The return value contains a boolean, whether the command was dispatched.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_WindowExecuteCommandAsync,
                            int /* automation handle */,
                            int /* browser command */,
                            bool /* success flag */)

// This message requests the execution of a browser command in the browser
// for which the handle is specified.
// The return value contains a boolean, whether the command was dispatched
// and successful executed.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_WindowExecuteCommand,
                            int /* automation handle */,
                            int /* browser command */,
                            bool /* success flag */)




















// This message starts a find within a tab corresponding to the supplied
// tab handle. The parameter |request| specifies what to search for.
// If an error occurs, |matches_found| will be -1.
//
IPC_SYNC_MESSAGE_CONTROL2_2(AutomationMsg_Find,
                            int /* tab_handle */,
                            AutomationMsg_Find_Params /* params */,
                            int /* active_ordinal */,
                            int /* matches_found */)

// Is the Find window fully visible (and not animating) for the specified
// tab?
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_FindWindowVisibility,
                            int /* tab_handle */,
                            bool /* is_visible */)








// Uses the specified encoding to override the encoding of the page in the
// specified web content tab.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_OverrideEncoding,
                            int /* tab handle */,
                            std::string /* overrided encoding name */,
                            bool /* success */)
























// This message requests the tabstrip index of the tab with the given handle.
// The return value contains the index, which will be -1 on failure.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_TabIndex,
                            int,
                            int)







// This message requests the number of normal browser windows, i.e. normal
// type and non-incognito mode that the app currently has open.  The return
// value is the number of windows.
IPC_SYNC_MESSAGE_CONTROL0_1(AutomationMsg_NormalBrowserWindowCount,
                            int)


































































IPC_MESSAGE_CONTROL1(AutomationMsg_ReloadAsync,
                     int /* tab handle */)

IPC_MESSAGE_CONTROL1(AutomationMsg_StopAsync,
                     int /* tab handle */)

// This message notifies the AutomationProvider to navigate to a specified
// url in the tab with given handle. The first parameter is the handle to
// the tab resource. The second parameter is the target url.  The third
// parameter is the number of navigations that are required for a successful
// return value. See AutomationMsg_NavigationResponseValues for the return
// value.
IPC_SYNC_MESSAGE_CONTROL3_1(
    AutomationMsg_NavigateToURLBlockUntilNavigationsComplete,
    int,
    GURL,
    int,
    AutomationMsg_NavigationResponseValues)












// This message requests the provider to wait until the window count
// reached the specified value.
// Request:
//  - int: target browser window count
// Response:
//  - bool: whether the operation was successful.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_WaitForBrowserWindowCountToBecome,
                           int,
                           bool)

// This message notifies the AutomationProvider to navigate back in session
// history in the tab with given handle. The first parameter is the handle
// to the tab resource. The second parameter is the number of navigations the
// provider will wait for.
// See AutomationMsg_NavigationResponseValues for the navigation response
// values.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_GoBackBlockUntilNavigationsComplete,
                            int,
                            int,
                            AutomationMsg_NavigationResponseValues)

// This message notifies the AutomationProvider to navigate forward in session
// history in the tab with given handle. The first parameter is the handle
// to the tab resource. The second parameter is the number of navigations
// the provider will wait for.
// See AutomationMsg_NavigationResponseValues for the navigation response
// values.
IPC_SYNC_MESSAGE_CONTROL2_1(
    AutomationMsg_GoForwardBlockUntilNavigationsComplete,
    int,
    int,
    AutomationMsg_NavigationResponseValues)














// Simulate an end of session. Normally this happens when the user
// shuts down the machine or logs off.
// Request:
//   int - the handle of the browser
// Response:
//   bool - true if succesful
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_TerminateSession,
                            int,
                            bool)

IPC_MESSAGE_CONTROL2(AutomationMsg_SetPageFontSize,
                     int /* tab_handle */,
                     int /* The font size */)

// Returns a metric event duration that was last recorded.  Returns -1 if the
// event hasn't occurred yet.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_GetMetricEventDuration,
                            std::string /* event_name */,
                            int /* duration ms */)

// Sent by automation provider - go to history entry via automation.
IPC_MESSAGE_ROUTED1(AutomationMsg_RequestGoToHistoryEntryOffset,
                    int)   // numbers of entries (negative or positive)

// This message requests the type of the window with the given handle. The
// return value contains the type (Browser::Type), or -1 if the request
// failed.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_Type,
                            int,
                            int)

// Opens a new browser window of a specific type.
IPC_SYNC_MESSAGE_CONTROL2_0(AutomationMsg_OpenNewBrowserWindowOfType,
                            int   /* Type (Browser::Type) */,
                            bool  /* show */ )











































// Wait for the bookmark model to load.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_WaitForBookmarkModelToLoad,
                            int /* browser_handle */,
                            bool /* success */)












































// Generic pyauto pattern to help avoid future addition of
// automation messages.
IPC_SYNC_MESSAGE_CONTROL2_2(AutomationMsg_SendJSONRequestWithBrowserHandle,
                            int /* browser_handle */,
                            std::string /* JSON request */,
                            std::string /* JSON response */,
                            bool /* success */)






























// Waits for tab count to reach target value.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_WaitForTabCountToBecome,
                            int /* browser handle */,
                            int /* target tab count */,
                            bool /* success */)

// Waits for the infobar count to reach given number.
IPC_SYNC_MESSAGE_CONTROL2_1(AutomationMsg_WaitForInfoBarCount,
                            int /* tab handle */,
                            size_t /* target count */,
                            bool /* success */)

// Notify the JavaScript engine in the render to change its parameters
// while performing stress testing.
IPC_MESSAGE_CONTROL3(AutomationMsg_JavaScriptStressTestControl,
                     int /* tab handle */,
                     int /* command */,
                     int /* type or run */)

// This message posts a task to the PROCESS_LAUNCHER thread. Once processed
// the response is sent back. This is useful when you want to make sure all
// changes to the number of processes have completed.
IPC_SYNC_MESSAGE_CONTROL0_0(AutomationMsg_WaitForProcessLauncherThreadToGoIdle)











// Call BeginTracing on the browser TraceController. This will tell all
// processes to start collecting trace events via base/debug/trace_event.h.
IPC_SYNC_MESSAGE_CONTROL1_1(AutomationMsg_BeginTracing,
                            std::string /* category_patterns */,
                            bool /* success */)

// End tracing (called after BeginTracing). This blocks until tracing has
// stopped on all processes and all the events are ready to be retrieved.
IPC_SYNC_MESSAGE_CONTROL0_2(AutomationMsg_EndTracing,
                            base::FilePath /* result_file_path */,
                            bool /* success */)

// Used on Mac OS X to read the number of active Mach ports used in the browser
// process.
IPC_SYNC_MESSAGE_CONTROL0_1(AutomationMsg_GetMachPortCount,
                            int /* number of Mach ports */)

// Generic pyauto pattern to help avoid future addition of
// automation messages.
IPC_SYNC_MESSAGE_CONTROL2_2(AutomationMsg_SendJSONRequest,
                            int /* window_index */,
                            std::string /* JSON request */,
                            std::string /* JSON response */,
                            bool /* success */)
