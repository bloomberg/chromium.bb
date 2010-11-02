// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CEEE_IE_BROKER_EVENT_DISPATCHING_DOCS_H_  // Mainly for lint
#define CEEE_IE_BROKER_EVENT_DISPATCHING_DOCS_H_

/** @page EventDispatchingDoc Detailed documentation of CEEE Event Dispatching.

@section EventDispatchingIntro Introduction

<a href="http://code.google.com/chrome/extensions/index.html">Chrome extensions
</a> can register to be notified of specific events like
<a href="http://code.google.com/chrome/extensions/windows.html#event-onCreated">
window creation</a> or
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onUpdated">
tab update</a>.

So CEEE needs to let Chrome know when these events occure in IE. To do so, we
unfortunately need to be hooked in a few different places. So in the same way
that the Chrome Extensions API implementation was described in @ref
ApiDispatcherDoc, this page describes all the notifications one by one and how
we implement their broadcasting.

Note that we use the EventFunnel class to properly package the arguments to be
sent with the events, and then send them to the Broker via the
ICeeeBroker::FireEvent method.

@section WindowsEvents Windows Events

There are three
<a href="http://code.google.com/chrome/extensions/windows.html#events">Windows
events</a> that can be sent to Chrome extensions,
<a href="http://code.google.com/chrome/extensions/windows.html#event-onCreated">
onCreated</a>, <a href=
"http://code.google.com/chrome/extensions/windows.html#event-onFocusChanged">
onFocusChanged</a>, and
<a href="http://code.google.com/chrome/extensions/windows.html#event-onRemoved">
onRemoved</a>.

These notifications are for top level windows only, so we can get notifications
from the OS by using the WH_SHELL hook (as described on
<a href="http://msdn.microsoft.com/en-us/library/ms644991(VS.85).aspx">
MSDN</a>). So we need to implement a hook function in the ceee_ie.dll so that
it can be injected in the system process and be called for the following
events:
<table>
<tr><td>HSHELL_WINDOWACTIVATED</td><td>Handle to the activated window.</td></tr>
<tr><td>HSHELL_WINDOWCREATED</td><td>Handle to the created window.</td></tr>
<tr><td>HSHELL_WINDOWDESTROYED</td><td>Handle to the destroyed window.</td></tr>
</table>

Then we must redirect those via Chrome Frame (more details in the @ref
ChromeFrameDispatching section). In the case of Windows Events, we must relay
the notifications to the Broker process via the ICeeeBrokerNotification
(TBD) interface since they get handled by an injected ceee_ie.dll.

@subsection onCreated

The onCreated event needs to send (as arguments to the notification message)
the same information about the Window as the one returned by the windows.create
API (and as described in the @ref ApiDispatcherDoc and on the <a href=
"http://code.google.com/chrome/extensions/windows.html#type-Window"> Chrome
Extensions documentation</a>).

@subsection onFocusChanged

All that is needed here is to send the window identifier of the newly focused
window to the listener.

@subsection onRemoved

All that is needed here is to send the window identifier of the removed window
to the listener.

@section TabsEvents Tabs Events

There are seven
<a href="http://code.google.com/chrome/extensions/tabs.html#events">Tabs
events</a> that can be sent to Chrome extensions,
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onAttached">
onAttached</a>,
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onCreated">
onCreated</a>,
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onDetached">
onDetached</a>,
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onMoved">
onMoved</a>,
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onRemoved">
onRemoved</a>, <a href=
"http://code.google.com/chrome/extensions/tabs.html#event-onSelectionChanged"
>onSelectionChanged</a>, and
<a href="http://code.google.com/chrome/extensions/tabs.html#event-onUpdated">
onUpdated</a>.

Since IE can't move tabs between windows (yet), CEEE won't fire the
attached/detached notifications. The tab created event can be fired when an
instance of the CEEE BHO is created and associated to a tab. The move, remove
and selected tab events will most likely need to be caught from IE's
TabWindowManager somehow (TBD). And finally, the update tab event can be
fired when the instance of the BHO attached to the given tab receives a
notification that the tab content was updated (via a series of notification from
the <a href="http://msdn.microsoft.com/en-us/library/aa752085(VS.85).aspx">
WebBrowser</a> or the <a href=
"http://msdn.microsoft.com/en-us/library/aa752574(VS.85).aspx">document</a>).

@note As a side note, here's an email from Siggi about tricks for this:

@note <em>2009/10/16 Sigurour Asgeirsson &lt;siggi@google.com&gt;</em>

@note I think we should be able to derive all the window events from tab
activity, if we e.g. have the BHO register each tab with its HWND/ID, then we
can derive the association to parent window from there, which means we have
created/destroyed on first registration and last unregistration for a frame
window.

@note I'm pretty sure we can derive the focus event from events fired on the
top-level browser associated with a BHO as well.

@subsection onAttached

Not implemented on IE.

@subsection onDetached

Not implemented on IE.

@subsection onMoved

TBD: We need to find a way to get the tab move notification from the
TabWindowManager... somehow... (e.g., patch the move method, but we would
rather find a notification coming from there instead).

We could also see if we can get win32 notifications of focus or window
visibility or things of the like.

Once we do, the arguments that must be sent with this notification are the
identifier of the tab, and a dictionary of moveInfo that contains the
"windowId", the "fromIndex" and the "toIndex" fields, which are all integers.

@subsection onRemoved

TBD: We need to find a way to get the tab removed notification from the
TabWindowManager... somehow...

We could also use the destruction of the BHO as a hint that the tab is being
removed.

Once we do, the argument that must be sent with this notification is just the
identifier of the tab.

@subsection onSelectionChanged

TBD: We need to find a way to get the tab selection changed notification from
the TabWindowManager... somehow...

We could also see if we can get win32 notifications of focus or window
visibility or things of the like.

Once we do, the arguments that must be sent with this notification are the
identifier of the tab that just got selected, and a dictionary of selectInfo
that only contains the "windowId" field.

@subsection onCreated

When a new BHO instance is created and attached to a tab, it can send this
notification directly to the Chrome Frame it is attached to. We will most likely
change this direct call to Chrome Frame once we implement the asynchronous
callback to Chrome Extension for the tabs.create API implementation for IE. At
that point, we should use the same ICeeeBrokerNotification (TBD) interface
as we plan on using for the @link WindowsEvents Windows Events @endlink.

The onCreated event needs to send to the extensions registered as listeners,
the same information about the Tab as the one returned by the tabs.create
API (and as described in the @ref ApiDispatcherDoc and on the <a href=
"http://code.google.com/chrome/extensions/tabs.html#type-Tab"> Chrome
Extensions documentation</a>).

@subsection onUpdated

When a new BHO instance receives a <a href=
"http://msdn.microsoft.com/en-us/library/aa768334(VS.85).aspx">NavigateComplete2
</a> notification from the WebBrowser, it must fire the onUpdated notification
for the tab it is attached to. It must also fire it for the ready state change
notifications it would get from the document. There's an edge case for when
IE refreshes the page, which can be caught when the frame ready state drops from
complete to loading, so we must also handle this case to send an onUpdated
notification for the tab.

It gets trickier when the page has embedded frames. These independent frames
will all have their own ready state, and changing the ready state (by
refreshing) a frame in the middle of the hierarchy, will change its own ready
state from complete, to loading, and then to interactive, and then the child
frames will go from uninitialized, to loading and then to interactive. The
innermost child will also go all the way to complete, and then its immediate
parent will get from interactive to complete, and then the next ancestor, all
the way to where the refresh started, but not further (so the outermost frame's
ready state will not change unless it is the one that triggered the refresh).

From the Chrome Extension's point of view, there should be only two events sent,
the initial one that goes from complete to loading, and the final one to go
from loading to complete. Since this doesn't necessary happen on the topmost
frame, we must listen to all the frames ready state changes, but we must be
able to compute the state of the whole page, so that we only send one
notification that would go from complete to loading at the beginning of the
refresh, and then only one that will go from loading to complete, once the
page is completely refreshed.

To do so, we need to propagate the state changes upward in the hierarchy and
parse the whole tree to confirm the page state, which is loading unless all
the frames are complete, in which case it will be complete. To minimize the
number of times we need to parse the hierarchy, only the top frame needs to
do the parsing. Also, frames don't need to propagate up their children's state
unless they are, themselves (the parent) in the complete state. If they are not,
then their own parent (or themselves if its the root) know that the overall
state isn't complete.

When we fire the onUpdated notification we must provide the identifier of the
tab and dictionary that contains the status field, which can be either "loading"
or "complete", as well as an optional URL field that the tab is set to show
(it is only provided if it changed).

@section BookmarksEvents Bookmarks Events

TBD

http://code.google.com/chrome/extensions/bookmarks.html#events

@section ExtensionsEvents Extensions Events

This is already available... more details TBD...

http://code.google.com/chrome/extensions/extension.html#events

@section ChromeFrameDispatching Dispatching to Chrome via the Chrome Frame

To simplify our access to (and reception of notifications from) Chrome Frame,
we have a ChromeFrameHost class that implements all we need. It takes care of
notification registration, and have virtual methods for all of them, and it
also has a PostMessage method to allow calls to be dispatched to Chrome via
the automation layer and Chrome Frame.

Events are such notifications that need to be sent, so the places where we
intercept IE notifications can use a ChromeFrameHost object, package the
event information in a JSON encoded string and pass it to the
ChromeFrameHost::PostMessage method. The JSON data must be a list with two
entries, the first one is the event name (e.g., "windows.onRemoved" or
"tabs.onUpdated") and the other one is the arguments for this event (e.g.,
a single int representing the id of the window to remove or for the
tabs.onUpdated event, a list of two values, the first one being an int for the
tab identifier, and the other one is a dictionary with a mandatory field for the
"status", which value can be "loading" or "complete", and an optional one for
the "url" which is a string reprensenting the URL to which the tab is being
navigated to).

The target of the message must be kAutomationBrowserEventRequestTarget (which is
"__priv_evtreq") and the origin, must, of course, be kAutomationOrigin (which is
"__priv_xtapi").

**/

#endif  // CEEE_IE_BROKER_EVENT_DISPATCHING_DOCS_H_
