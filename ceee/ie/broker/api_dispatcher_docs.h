// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CEEE_IE_BROKER_API_DISPATCHER_DOCS_H_  // Mainly for lint
#define CEEE_IE_BROKER_API_DISPATCHER_DOCS_H_

/** @page ApiDispatcherDoc Detailed documentation of the ApiDispatcher.

@section ApiDispatcherIntro Introduction

The API Dispatcher implemented by the ApiDispatcher class
is used by the CeeeBroker object to dispatch execution of Api calls
coming from the "Chrome Extensions Execution Environment" or CEEE.

It keeps a registry of all API invocation implementations and takes care of
the logic needed to de-serialize API function requests, dispatch them,
and then serialize & return the response.

One of the main complexities of the API invocation implementations is that some
of them need to interact with windows, and these interactions must happen in
the same thread that created the window. So the dispatcher need to delegate
calls to Executor objects that they get from the ExecutorsManager.

So the implementation of these APIs are actually split between the classes
inheriting from ApiDispatcher::Invocation and the CeeeExecutor COM object.

Another complexity is the fact that some of the APIs can not be completed
synchronously (e.g., the tab creation is an asynchronous process), so the
dispatcher must remember the pending execution and get them completed when a
given event is fired.

And to make it even more fun, there might be two concurrent API invocation
that depend on the same event being fired (e.g., GetSelectedTab is called
while a new tab is being created as selected, but the asynchronous selection
isn't completed yet... pfffewww). So the list of pending asynchronous execution
must allow more than one to be executed when a given event is fired.

@section InvocationFactory Invocation Factory

The ApiDispatcher uses the @link ApiDispatcher::Invocation Invocation @endlink
nested class as a base for all API invocation implementation so that it can
easily dispatch to them. It keeps pointers to registered static functions
(called Invocation factories) that can be called to instantiation a new
Invocation object in the @link ApiDispatcher::factories_ factories_ @endlink
@link ApiDispatcher::FactoryMap map@endlink.

When @link ApiDispatcher::HandleApiRequest HandleApiRequest @endlink is called,
it looks up the Invocation Factory to find the Invocation object to execute with
the data that it de-serialized from the JSON string it was given.

@subsection InvocationExecution Invocation Execution

The Invocation base class has an @link ApiDispatcher::Invocation::Execute
Execute @endlink pure virtual method that is to be called by HandleApiRequest.
This method receives the argument values, as well as a pointer to a string
where to return the response.

When the execution can be completed synchronously, the result (or the error
message in case of failure) is written in the @link
ApiDispatcher::Invocation::result_ result_ @endlink member variable
(or @link ApiDispatcher::Invocation::error_ error_ @endlink for errors). If the
execution must be completed asynchronously, a new AsynchronousHandler object
is instantiated and stored in the @link
ApiDispatcher::Invocation::async_handler_ async_handler_ @endlink data member
so that the dispatcher can find it.

@subsection ExecutorsDelegation Delegating to Executors

When an Invocation implementation needs to interact with windows, it can do so
by delegating the execution to an Executor object via the ICeeeTabExecutor
or ICeeeWindowExecutor interface pointer that it can get from a call to
ApiDispatcher::Invocation::GetExecutor which gets it from the Executors Manager.

@section EventHandling Event Handling

Event handling actually serves two purposes in the API Dispatcher, 1) handle
events (of course) and 2) complete asynchronous executions. Events are received
by the @link ApiDispatcher::FireEvent FireEvent @endlink member function.

@subsection HandlingEvents Handling Events

Events are being fired by the browser and must be propagated to extensions via
the Chrome Frame. The @link EventDispatchingDoc Event Dispatching @endlink
documentation describes how the EventFunnel derived classes can be used to help
fire events. Once they got through the funnel, the events get to the API
Dispatcher, which is responsible to post them to Chrome Frame.

We do the event propagation to Chrome Frame from the API dispatcher so that
we can also deal with the completion of the asynchronous API invocation at the
same time... the same place...

We also reuse some of the code written for API invocation since some of the
events are posted with the same information that is returned to the API
invocation callbacks (e.g., tabs.create and tabs.onCreated).

So the ApiDispatcher::Invocation derived classes can be used to handle events
by filling in the blanks in the arguments posted with the event (because some of
these arguments can be filled by the source of the event, we only need to fill
in the blanks that were left because, for example, the information must be
fetched from other threads than the one from where the event come from). Those
classes that can be used to handle events must expose a static method with the
syntax defined as ApiDispatcher::EventHandler and register it by calling
ApiDispatcher::RegisterEventHandler so that it get stored in the
ApiDispatcher::EventHandlersMap.

When ApiDispatcher::FireEvent is called, the event handlers map is looked up to
see if a handler was registered for the given event. If so, the event handler
is called to fill in the blanks in the event arguments. Then the newly filled
argumentes can be posted with the event.

@subsection AsynchronousResponse Asynchronous API Invocation response

When an API invocation can't be completed synchronously, a new instance of an
ApiDispatcher::AsynchronousHandler object is stored in the
ApiDispatcher::AsyncHandlersMap. These objects are associated to a given
event (which can be identified by calling the
ApiDispatcher::AsyncHandlersMap::event_name member function). So when an event
is fired, the map is looked up for the list of handlers registered for the
given event name (because there can be more than one handler registerd for a
given name, for the cases of concurrent API invocations, and also for cases
where more than one invocation need to wait for the same event, e.g., the
tabs.create and tabs.getSelected APIs).

Since there may be more than one handler for a given event, and we may have
pending handlers for more than one event with the same name, we need to be able
to properly match the events with their pending handlers. To identify if an
event is the one a specific handler is waiting for, we have to ask the handlers
if they recognize it as theirs. Even if they do, we may still need to let
other handlers process the same event. This is why a handler can specify that
it is a pass-through handler (i.e., it doesn't swallow the event, it can let
other handlers use it, even multiple instances of itself can all use the same
one, as long as they recognize it as theirs). If a handler recognize an event
as its own, and isn't a pass-through handler, then, we must stop looking for
more handlers.

This means that all pass-through handlers must be placed at the beginning of
the list of handlers for a given event, and all the other ones (the
non-pass-through handlers) must be put at the end.

@section InvocationImplementation Invocation Implementation

The implementation of API invocations are done by classes derived from the
Invocation nested class of the ApiDispatcher class. Here is the current list of
those derived classes and some details about their implementation.

@subsection WindowsApi Windows API

Here is the list of invocations implementing the Windows API.
You can also read a more detailed information about the Chrome Extension
Windows api here: http://code.google.com/chrome/extensions/windows.html

@subsubsection GetWindow

This invocation receives a single argument which is the identifier of the window
to return information for. The window identifier is simply the window's HWND
value. We only provide such information for IEFrame windows (so we validate
that before continuing) and since we must get this information from the thread
that created the window, we delegate this call to the @link
ICeeeWindowExecutor::GetWindow @endlink method of the
ICeeeWindowExecutor interface.

The returned information contains the following:
<table>
  <tr><td>type</td><td>Info</td></tr>
  <tr><td>int</td><td>Window identifier</td></tr>
  <tr><td>bool</td><td>Focused</td></tr>
  <tr><td>int</td><td>Left position</td></tr>
  <tr><td>int</td><td>Top position</td></tr>
  <tr><td>int</td><td>Width</td></tr>
  <tr><td>int</td><td>Height</td></tr>
</table>

@subsubsection GetCurrentWindow

This invocation does the same thing as @ref GetWindow except that it doesn't
receive a windows identifier argument (it doesn't receive any argument). Instead
it must find the current window from where the invocation started from.

@note We currently don't have a way to find the window from which the invocation
came from, so until we do, we return the top most IE Frame window as
@ref GetLastFocusedWindow does.

@subsubsection GetLastFocusedWindow

This invocation return information about the last focused IE window, which it
gets by calling @link window_api::WindowApiResult::TopIeWindow
TopIeWindow@endlink, which finds it by calling
window_utils::FindDescendentWindow with NULL as the parent (which means to ask
for top level windows).

@subsubsection CreateWindowFunc

This invocation creates a new window with the parameters specified in the
received argument and navigate it to an optionally provided URL or about:blank
otherwise. It creates the new window by simply calling the
ICeeeTabExecutor::Navigate method with a _blank target and the
navOpenInNewWindow flag.

It then needs to find that window (by diff'ing the list of IEFrame windows
before and after the navigation), and then it can apply the provided parameters.

The provided parameters are as follows (and are all optional):
<table>
<tr><td>type</td><td>Info</td></tr>
<tr><td>string</td><td>URL</td></tr>
<tr><td>int</td><td>Left position</td></tr>
<tr><td>int</td><td>Top position</td></tr>
<tr><td>int</td><td>Width</td></tr>
<tr><td>int</td><td>Height</td></tr>
</table>

The application of the window parameters must be done in the same thread that
created the window, so we must delegate this task to the @link
ICeeeWindowExecutor::UpdateWindow UpdateWindow @endlink method of
the ICeeeWindowExecutor interface. This UpdateWindow method also returns
information about the updated window so that we can return it. The returned
information is the same as the one returned by the @ref GetWindow method
described above.

@subsubsection UpdateWindow

This invocation updates an existing window. So it must be given the window
identifier, as well as the new parameters to update the window with.

It simply reuses the same code as the @ref CreateWindowFunc invocation, except
that it doesn't create a new window, it modifies the one specified by the
identifier, after validating that it is indeed an IEFrame window.

@subsubsection RemoveWindow

This invocation simply closes the window specified as a windows identifier
argument. It simply delegates the call to the @link
ICeeeWindowExecutor::RemoveWindow RemoveWindow @endlink method of the
ICeeeWindowExecutor interface.

@subsubsection GetAllWindows

This invocation returns an array of window information, one for each of the
current IEFrame window. It also fills an extra array of tabs information if the
populate flag is set in the single (and optional, defaults to false) argument.

The information returned for each window is the same as in the @ref GetWindow
invocation. The information for each tab (if requested) is provided as an extra
entry in the window information in the form of an array of tab information. The
tab information is the same as the one returned by the @ref GetTab invocation
described below.

It delegates the call to the @link ICeeeWindowExecutor::GetAllWindows
GetAllWindows @endlink method of the ICeeeWindowExecutor interface. This
methods can't return all the information about every tab since the windows for
these tabs may have been created in other threads/processes. So it only returns
the identifiers of these tabs. Then we must delegate to different executors for
each of the tabs potentially created in different threads/process by calling the
@link ICeeeTabExecutor::GetTab GetTab @endlink method of the
ICeeeTabExecutor interface as is done for the @ref GetTab API described
below. We do the same in @ref GetAllTabsInWindow described below.

@subsection TabsApi Tabs API

Here is the list of invocations implementing the Tabs API.
You can also read a more detailed information about the Chrome Extension
Tabs api here: http://code.google.com/chrome/extensions/tabs.html

@subsubsection GetTab

This invocation receives a single argument which is the identifier of the tab
to return information for. The tab identifier is the HWND value of the window
of class "TabWindowClass" that holds on the real tab window.

We only provide such information for these windows (so we validate
that before continuing). Also, we must get some of the information from the
thread that created the tab window, and some other information are only
available via the tab window manager which lives in the thread that created the
frame window. So we must delegate this call in two phases, the first
one to the @link ICeeeTabExecutor::GetTab GetTab @endlink method of the
ICeeeTabExecutor interface which returns the following information:
<table>
<tr><td>type</td><td>Info</td></tr>
<tr><td>string</td><td>URL</td></tr>
<tr><td>string</td><td>Title</td></tr>
<tr><td>string</td><td>Loading/Complete status</td></tr>
<tr><td>string</td><td>favIconUrl (not supported yet in CEEE)</td></tr>
</table>

The information above is retrivied via the HTML Document and Web Browser
associated to the given tab, and these must be fetched and interacted with from
the same thread that created the tab window.

So we are left with the following:
<table>
<tr><td>type</td><td>Info</td></tr>
<tr><td>int</td><td>Tab identifier</td></tr>
<tr><td>int</td><td>Window identifier</td></tr>
<tr><td>bool</td><td>Selected state</td></tr>
<tr><td>int</td><td>Tab index</td></tr>
</table>

Most of the other information about the tab can be retrieved from any thread
by simply using Win32 calls. The tab identifier is the HWND of the window of
class "TabWindowClass", the window identifier is simply the HWND of
the top level parent of the tab window, and the selected state can be determined
by the visibility of the window which we can get from anywhere.

The last piece of information, the tab index, must be retrieved from the tab
window manager. We must get another executor so that we can
run it in the appropriate thread. We do this via the @link
ICeeeTabExecutor::GetTabIndex GetTabIndex @endlink method of the
ICeeeTabExecutor interface.

@subsubsection GetSelectedTab

This invocation does the same thing as @ref GetTab except that it doesn't
receive a tab identifier argument, it gets the information of the currently
selected tab instead. But it can receive an optional window identifier argument,
specifying which window's selected tab we want. If no window identifier is
specified, we use the current window. The current window is the same as the one
returned from @ref GetCurrentWindow.

@subsubsection GetAllTabsInWindow

This invocation returns the information for all the tabs of a given window. If
no window identifier is provided as an optional argument, then the current
window is used instead. The returned information is an array of tab information.
The tab information is the same as the one returned by @ref GetTab.

We delegate the first part of this call to the
@link ICeeeTabExecutor::GetAllTabs GetAllTabs @endlink method of the
ICeeeTabExecutor interface which gets the list of tabs from the tab window
manager in the frame window thread. Then we need to sequentially call different
instances of executors to call their @ref GetTab method, since they were not
created in the same thread where GetAllTabs needs to be executed.

@subsubsection CreateTab

This invocation creates a new tab with the parameters specified in the received
argument and navigate it to an optionally provided URL or about:blank otherwise.

The new tab is to be created in a specified window or the current window if the
optional window identifier is not provided as an argument. The new tab
is created by first finding an existing tab in the any window and then
calling ICeeeTabExecutor::Navigate with the appropriate URL with the _blank
target and the navOpenInNewTab or navOpenInBackgroundTab whether the selected
bool optional parameter is set to true or not (defaults to true).

The new tab may also need to be moved to a specific index if the optional index
parameter argument was provided. This can be done using the tab window manager
so must be done in the appropriate thread by delegating this task to the @link
ICeeeTabExecutor::MoveTab MoveTab @endlink method of the
ICeeeTabExecutor interface.

And finally, the same tab information as from @ref GetTab must be returned.
Since some of this information isn't available right after the call to Navigate,
we must handle the rest of the CreateTab API invocation Asynchronously. We
register the async handler to be called when the tabs.onCreated event is fired.
Then, we know that all the information is available to return the tab info.

The provided parameters are as follows (and are all optional):
<table>
<tr><td>type</td><td>Info</td></tr>
<tr><td>int</td><td>Parent window identifier</td></tr>
<tr><td>int</td><td>Index</td></tr>
<tr><td>string</td><td>URL</td></tr>
<tr><td>bool</td><td>Selected</td></tr>
</table>

@note <b>TODO(mad@chromium.org)</b>: We currently can't get the tab id of the
newly created tab since it is created asynchronously. So we will need to return
this info asynchronously once we have the support for asynchronous events.

@subsubsection UpdateTab

This invocation updates an existing tab. So it must be given the tab identifier,
as well as the new parameters to update the tab with.

If the optional URL parameter is provided as an argument, then we must call
ICeeeTabExecutor::Navigate with this new URL (if it is different from the
current one).

If the optional selected parameter is provided, then we must use the tab window
manager to select the tab. But we must do it in the IE Frame thread so we must
delegate the tab selection to the @link ICeeeTabExecutor::SelectTab
SelectTab @endlink method of another instance of
the executor object implementing the ICeeeTabExecutor interface, since it
must be execute in the thread that created the frame window, this is the one we
must use when accessing the ITabWindowManager interface.

No information is returned from UpdateTab.

@subsubsection MoveTab

This invocation moves the tab from one index to another. So it must receive the
identifier of the tab, as well as the destination index. An optional window
identifier can also be provided if we want to specify in which window we want
to move the tab. When not specified, the window where the tab currently lies is
used (and it is the only case that is currently supported in CEEE).

This code is delegated to the @link ICeeeTabExecutor::MoveTab MoveTab
@endlink method of the ICeeeTabExecutor interface.

No information is returned from MoveTab.

@subsubsection RemoveTab

This invocation closes a tab identified by the single mandatory parameter
argument. The removal of the tab is delegated to the ITabWindowManager interface
so it must be executed in the IEFrame thread by calling the @link
ICeeeTabExecutor::RemoveTab RemoveTab @endlink method on
the ICeeeTabExecutor interface.

@note <b>TODO(mad@chromium.org)</b>: An alternative to having the RemoveTab and
MoveTab and SelectTab methods on the ICeeeTabExecutor interface, would be to
simply have a GetTabWindowManager method which returns a proxy to the
ITabWindowManager interface that we can safely use from the Broker process.

No information is returned from RemoveTab.

**/

#endif  // CEEE_IE_BROKER_API_DISPATCHER_DOCS_H_
