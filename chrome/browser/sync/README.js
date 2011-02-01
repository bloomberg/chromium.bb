Overview of chrome://sync-internals
-----------------------------------

This note explains how chrome://sync-internals (also known as
about:sync) interacts with the sync service/backend.

Basically, chrome://sync-internals sends asynchronous messages to the
sync backend and the sync backend asynchronously raises events to
chrome://sync-internals, either when replying to messages or when
something interesting happens.

Both messages and events have a name and a list of arguments, the
latter of which is represented by a JsArgList (js_arg_list.h) object,
which is basically a wrapper around an immutable ListValue.

Message/event flow
------------------

chrome://sync-internals is represented by SyncInternalsUI
(chrome/browser/dom_ui/sync_internals_ui.h).  SyncInternalsUI
interacts with the sync service via a JsFrontend (js_frontend.h)
object, which has a ProcessMessage() method.  The JsFrontend can
handle some messages itself, but it can also delegate the rest to a
JsBackend instance (js_backend.h), which also has a ProcessMessage()
method.  A JsBackend can in turn handle some messages itself and
delegate to other JsBackend instances.

Essentially, there is a tree with a JsFrontend as the root and
JsBackend as non-root internal nodes and leaf nodes (although
currently, the tree is more like a simple list).  The sets of messages
handled by the JsBackends and the JsFrontend are disjoint, which means
that at most one node handles a given message type.  Also, the
JsBackends may live on different threads, but JsArgList is thread-safe
so that's okay.

SyncInternalsUI is a JsEventHandler (js_event_handler.h), which means
that it has a HandleJsEvent() method, but JsBackends cannot easily
access those objects.  Instead, each JsBackend keeps track of its
parent router, which is a JsEventRouter object (js_event_router.h).
Basically, a JsEventRouter is another JsBackend object or a JsFrontend
object.  So an event travels up through the JsEventRouter until it
reaches the JsFrontend, which knows about the existing JsEventHandlers
(via AddHandler()/RemoveHandler()) and so can delegate to the right
one.

A diagram of the flow of a message and its reply:

msg(args) -> F -> B -> B -> B
             |    |    |
        H <- R <- R <- R <- reply-event(args)

F = JsFrontend, B = JsBackend, R = JsEventRouter, H = JsEventHandler

Non-reply events are percolated up similarly.
