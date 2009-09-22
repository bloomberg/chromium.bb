// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/common/dom_storage_type.h"
#include "ipc/ipc_message.h"

class DOMStorageContext;
class WebKitThread;

// This class handles the logistics of DOM Storage within the browser process.
// It mostly ferries information between IPCs and the WebKit implementations,
// but it also handles some special cases like when renderer processes die.
class DOMStorageDispatcherHost :
    public base::RefCountedThreadSafe<DOMStorageDispatcherHost> {
 public:
  // Only call the constructor from the UI thread.
  DOMStorageDispatcherHost(IPC::Message::Sender* message_sender,
                           WebKitContext*, WebKitThread*);

  // Only call from ResourceMessageFilter on the IO thread.
  void Shutdown();
  bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);

  // Send a message to the renderer process associated with our
  // message_sender_ via the IO thread.  May be called from any thread.
  void Send(IPC::Message* message);

 private:
  friend class base::RefCountedThreadSafe<DOMStorageDispatcherHost>;
  ~DOMStorageDispatcherHost();

  // Message Handlers.
  void OnNamespaceId(DOMStorageType storage_type, IPC::Message* reply_msg);
  void OnCloneNamespaceId(int64 namespace_id, IPC::Message* reply_msg);
  void OnStorageAreaId(int64 namespace_id, const string16& origin,
                       IPC::Message* reply_msg);
  void OnLength(int64 storage_area_id, IPC::Message* reply_msg);
  void OnKey(int64 storage_area_id, unsigned index, IPC::Message* reply_msg);
  void OnGetItem(int64 storage_area_id, const string16& key,
                 IPC::Message* reply_msg);
  void OnSetItem(int64 storage_area_id, const string16& key,
                 const string16& value);
  void OnRemoveItem(int64 storage_area_id, const string16& key);
  void OnClear(int64 storage_area_id);

  // A shortcut for accessing our context.
  DOMStorageContext* Context() {
    DCHECK(!shutdown_);
    return webkit_context_->GetDOMStorageContext();
  }

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // ResourceDispatcherHost takes care of destruction.  Immutable.
  WebKitThread* webkit_thread_;

  // Only set on the IO thread.
  IPC::Message::Sender* message_sender_;

  // Has this dispatcher ever handled a message.  If not, then we can skip
  // the entire shutdown procedure.  This is only set to true on the IO thread
  // and must be true if we're reading it on the WebKit thread.
  bool ever_used_;

  // This is set once the Shutdown routine runs on the WebKit thread.  Once
  // set, we should not process any more messages because storage_area_map_
  // and storage_namespace_map_ contain pointers to deleted objects.
  bool shutdown_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageDispatcherHost);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
