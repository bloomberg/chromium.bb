Overview of chrome://sync-internals
-----------------------------------

This note explains how chrome://sync-internals (also known as
about:sync) interacts with the sync service/backend.

Basically, chrome://sync-internals sends messages to the sync backend
and the sync backend sends the reply asynchronously.  The sync backend
also asynchronously raises events which chrome://sync-internals listen
to.

A message and its reply has a name and a list of arguments, which is
basically a wrapper around an immutable ListValue.

An event has a name and a details object, which is represented by a
JsEventDetails (js_event_details.h) object, which is basically a
wrapper around an immutable DictionaryValue.

TODO(akalin): Move all the js_* files into a js/ subdirectory.

Message/event flow
------------------

chrome://sync-internals is represented by SyncInternalsUI
(chrome/browser/ui/webui/sync_internals_ui.h).  SyncInternalsUI
interacts with the sync service via a JsController (js_controller.h)
object, which has a ProcessJsMessage() method that just delegates to
an underlying JsBackend instance (js_backend.h).  The SyncInternalsUI
object also registers itself (as a JsEventHandler
[js_event_handler.h]) to the JsController object, and any events
raised by the JsBackend are propagated to the JsController and then to
the registered JsEventHandlers.

The ProcessJsMessage() takes a WeakHandle (weak_handle.h) to a
JsReplyHandler (js_reply_handler.h), which the backend uses to send
replies safely across threads.  SyncInternalsUI implements
JsReplyHandler, so it simply passes itself as the reply handler when
it calls ProcessJsMessage() on the JsController.

The following objects live on the UI thread:

- SyncInternalsUI (implements JsEventHandler, JsReplyHandler)
- SyncJsController (implements JsController, JsEventHandler)

The following objects live on the sync thread:

- SyncManager::SyncInternal (implements JsBackend)

Of course, none of these objects need to know where the other objects
live, since they interact via WeakHandles.
