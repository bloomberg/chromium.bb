// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_DISPATCHER_HOST_H_

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

class WebKitContext;
class WebKitThread;

namespace WebKit {
class WebStorageArea;
class WebStorageNamespace;
class WebString;
}

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
  void OnNamespaceId(bool is_local_storage, IPC::Message* reply_msg);
  void OnCloneNamespaceId(int64 namespace_id, IPC::Message* reply_msg);
  void OnDerefNamespaceId(int64 namespace_id);
  void OnStorageAreaId(int64 namespace_id, const string16& origin,
                       IPC::Message* reply_msg);
  void OnLock(int64 storage_area_id, IPC::Message* reply_msg);
  void OnUnlock(int64 storage_area_id);
  void OnLength(int64 storage_area_id, IPC::Message* reply_msg);
  void OnKey(int64 storage_area_id, unsigned index, IPC::Message* reply_msg);
  void OnGetItem(int64 storage_area_id, const string16& key,
                 IPC::Message* reply_msg);
  void OnSetItem(int64 storage_area_id, const string16& key,
                 const string16& value);
  void OnRemoveItem(int64 storage_area_id, const string16& key);
  void OnClear(int64 storage_area_id, IPC::Message* reply_msg);

  // Get a WebStorageNamespace or WebStorageArea based from its ID.  Only call
  // on the WebKit thread.
  WebKit::WebStorageArea* GetStorageArea(int64 id);
  WebKit::WebStorageNamespace* GetStorageNamespace(int64 id);

  // Add a WebStorageNamespace or WebStorageArea and get a new unique ID for
  // it.  Only call on the WebKit thread.
  int64 AddStorageArea(WebKit::WebStorageArea* new_storage_area);
  int64 AddStorageNamespace(WebKit::WebStorageNamespace* new_namespace);

  // Get the path to the LocalStorage directory.  Calculate it if we haven't
  // already.  Only call on the WebKit thread.
  WebKit::WebString GetLocalStoragePath();

  // Data shared between renderer processes with the same profile.
  scoped_refptr<WebKitContext> webkit_context_;

  // ResourceDispatcherHost takes care of destruction.  Immutable.
  WebKitThread* webkit_thread_;

  // Only set on the IO thread.
  IPC::Message::Sender* message_sender_;

  // The last used storage_area_id and storage_namespace_id's.  Only use on the
  // WebKit thread.
  int64 last_storage_area_id_;
  int64 last_storage_namespace_id_;

  // Used to maintain a mapping between storage_area_id's used in IPC messages
  // and the actual WebStorageArea instances.  Only use on the WebKit thread.
  typedef base::hash_map<int64, WebKit::WebStorageArea*> StorageAreaMap;
  StorageAreaMap storage_area_map_;

  // Mapping between storage_namespace_id's used in IPC messages and the
  // WebStorageNamespace instances.  Only use on the WebKit thread.
  typedef base::hash_map<int64, WebKit::WebStorageNamespace*>
      StorageNamespaceMap;
  StorageNamespaceMap storage_namespace_map_;

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
