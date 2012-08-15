// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"

class GURL;
class IndexedDBContextImpl;
struct IndexedDBDatabaseMetadata;
struct IndexedDBHostMsg_DatabaseCreateObjectStore_Params;
struct IndexedDBHostMsg_FactoryDeleteDatabase_Params;
struct IndexedDBHostMsg_FactoryGetDatabaseNames_Params;
struct IndexedDBHostMsg_FactoryOpen_Params;
struct IndexedDBHostMsg_IndexCount_Params;
struct IndexedDBHostMsg_IndexOpenCursor_Params;
struct IndexedDBHostMsg_ObjectStoreCount_Params;
struct IndexedDBHostMsg_ObjectStoreCreateIndex_Params;
struct IndexedDBHostMsg_ObjectStoreOpenCursor_Params;
struct IndexedDBHostMsg_ObjectStorePut_Params;

namespace WebKit {
class WebDOMStringList;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBIndex;
class WebIDBObjectStore;
class WebIDBTransaction;
struct WebIDBMetadata;
}

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class SerializedScriptValue;
}

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost : public content::BrowserMessageFilter {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(int process_id,
                          IndexedDBContextImpl* indexed_db_context);

  // content::BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void TransactionComplete(int32 transaction_id);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* Context() { return indexed_db_context_; }

  // The various IndexedDBCallbacks children call these methods to add the
  // results into the applicable map.  See below for more details.
  int32 Add(WebKit::WebIDBCursor* idb_cursor);
  int32 Add(WebKit::WebIDBDatabase* idb_database,
            int32 thread_id,
            const GURL& origin_url);
  int32 Add(WebKit::WebIDBIndex* idb_index);
  int32 Add(WebKit::WebIDBObjectStore* idb_object_store);
  int32 Add(WebKit::WebIDBTransaction* idb_transaction,
            int32 thread_id,
            const GURL& origin_url);
  int32 Add(WebKit::WebDOMStringList* domStringList);

  WebKit::WebIDBCursor* GetCursorFromId(int32 cursor_id);

 private:
  virtual ~IndexedDBDispatcherHost();

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnIDBFactoryGetDatabaseNames(
      const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& p);
  void OnIDBFactoryOpen(const IndexedDBHostMsg_FactoryOpen_Params& p);

  void OnIDBFactoryDeleteDatabase(
      const IndexedDBHostMsg_FactoryDeleteDatabase_Params& p);

  void ResetDispatcherHosts();

  // Helper templates.
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(
    IDMap<ReturnType, IDMapOwnPointer>* map, int32 return_object_id);

  template <typename ObjectType>
  void DestroyObject(IDMap<ObjectType, IDMapOwnPointer>* map, int32 object_id);

  // Used in nested classes.
  typedef std::map<int32, GURL> WebIDBObjectIDToURLMap;
  typedef std::map<int32, int64> WebIDBTransactionIDToSizeMap;

  class DatabaseDispatcherHost {
   public:
    explicit DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnMetadata(int32 idb_database_id,
                    IndexedDBDatabaseMetadata* metadata);
    void OnCreateObjectStore(
        const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params,
        int32* object_store_id, WebKit::WebExceptionCode* ec);
    void OnDeleteObjectStore(int32 idb_database_id,
                             const string16& name,
                             int32 transaction_id,
                             WebKit::WebExceptionCode* ec);
    void OnSetVersion(int32 idb_database_id,
                      int32 thread_id,
                      int32 response_id,
                      const string16& version,
                      WebKit::WebExceptionCode* ec);
    void OnTransaction(int32 thread_id,
                       int32 idb_database_id,
                       const std::vector<string16>& names,
                       int32 mode,
                       int32* idb_transaction_id,
                       WebKit::WebExceptionCode* ec);
    void OnOpen(int32 idb_database_id, int32 thread_id, int32 response_id);
    void OnClose(int32 idb_database_id);
    void OnDestroyed(int32 idb_database_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer> map_;
    WebIDBObjectIDToURLMap database_url_map_;
  };

  class IndexDispatcherHost {
   public:
    explicit IndexDispatcherHost(IndexedDBDispatcherHost* parent);
    ~IndexDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnOpenObjectCursor(
        const IndexedDBHostMsg_IndexOpenCursor_Params& params,
        WebKit::WebExceptionCode* ec);
    void OnOpenKeyCursor(const IndexedDBHostMsg_IndexOpenCursor_Params& params,
                         WebKit::WebExceptionCode* ec);
    void OnCount(const IndexedDBHostMsg_IndexCount_Params& params,
                 WebKit::WebExceptionCode* ec);
    void OnGetObject(int idb_index_id,
                     int32 thread_id,
                     int32 response_id,
                     const content::IndexedDBKeyRange& key_range,
                     int32 transaction_id,
                     WebKit::WebExceptionCode* ec);
    void OnGetKey(int idb_index_id,
                  int32 thread_id,
                  int32 response_id,
                  const content::IndexedDBKeyRange& key_range,
                  int32 transaction_id,
                  WebKit::WebExceptionCode* ec);
    void OnDestroyed(int32 idb_index_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBIndex, IDMapOwnPointer> map_;
  };

  class ObjectStoreDispatcherHost {
   public:
    explicit ObjectStoreDispatcherHost(IndexedDBDispatcherHost* parent);
    ~ObjectStoreDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnGet(int idb_object_store_id,
               int32 thread_id,
               int32 response_id,
               const content::IndexedDBKeyRange& key_range,
               int32 transaction_id,
               WebKit::WebExceptionCode* ec);
    void OnPut(const IndexedDBHostMsg_ObjectStorePut_Params& params,
               WebKit::WebExceptionCode* ec);
    void OnSetIndexKeys(
        int32 idb_object_store_id,
        const content::IndexedDBKey& primary_key,
        const std::vector<string16>& index_names,
        const std::vector<std::vector<content::IndexedDBKey> >& index_keys,
        int32 transaction_id);
    void OnSetIndexesReady(int32 idb_object_store_id,
                           const std::vector<string16>& names,
                           int32 transaction_id);

    void OnDelete(int idb_object_store_id,
                  int32 thread_id,
                  int32 response_id,
                  const content::IndexedDBKeyRange& key_range,
                  int32 transaction_id,
                  WebKit::WebExceptionCode* ec);
    void OnClear(int idb_object_store_id,
                 int32 thread_id,
                 int32 response_id,
                 int32 transaction_id,
                 WebKit::WebExceptionCode* ec);
    void OnCreateIndex(
        const IndexedDBHostMsg_ObjectStoreCreateIndex_Params& params,
        int32* index_id,
        WebKit::WebExceptionCode* ec);
    void OnIndex(int32 idb_object_store_id,
                 const string16& name,
                 int32* idb_index_id,
                 WebKit::WebExceptionCode* ec);
    void OnDeleteIndex(int32 idb_object_store_id,
                       const string16& name,
                       int32 transaction_id,
                       WebKit::WebExceptionCode* ec);
    void OnOpenCursor(
        const IndexedDBHostMsg_ObjectStoreOpenCursor_Params& params,
        WebKit::WebExceptionCode* ec);
    void OnCount(const IndexedDBHostMsg_ObjectStoreCount_Params& params,
                 WebKit::WebExceptionCode* ec);
    void OnDestroyed(int32 idb_object_store_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBObjectStore, IDMapOwnPointer> map_;
  };

  class CursorDispatcherHost {
   public:
    explicit CursorDispatcherHost(IndexedDBDispatcherHost* parent);
    ~CursorDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnUpdate(int32 idb_object_store_id,
                  int32 thread_id,
                  int32 response_id,
                  const content::SerializedScriptValue& value,
                  WebKit::WebExceptionCode* ec);
    void OnAdvance(int32 idb_object_store_id,
                   int32 thread_id,
                   int32 response_id,
                   unsigned long count,
                   WebKit::WebExceptionCode* ec);
    void OnContinue(int32 idb_object_store_id,
                    int32 thread_id,
                    int32 response_id,
                    const content::IndexedDBKey& key,
                    WebKit::WebExceptionCode* ec);
    void OnPrefetch(int32 idb_cursor_id,
                    int32 thread_id,
                    int32 response_id,
                    int n,
                    WebKit::WebExceptionCode* ec);
    void OnPrefetchReset(int32 idb_cursor_id, int used_prefetches,
                         int unused_prefetches);
    void OnDelete(int32 idb_object_store_id,
                  int32 thread_id,
                  int32 response_id,
                  WebKit::WebExceptionCode* ec);
    void OnDestroyed(int32 idb_cursor_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBCursor, IDMapOwnPointer> map_;
  };

  class TransactionDispatcherHost {
   public:
    explicit TransactionDispatcherHost(IndexedDBDispatcherHost* parent);
    ~TransactionDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnCommit(int32 transaction_id);
    void OnAbort(int32 transaction_id);
    void OnObjectStore(int32 transaction_id,
                       const string16& name,
                       int32* object_store_id,
                       WebKit::WebExceptionCode* ec);
    void OnDidCompleteTaskEvents(int transaction_id);
    void OnDestroyed(int32 idb_transaction_id);

    IndexedDBDispatcherHost* parent_;
    typedef IDMap<WebKit::WebIDBTransaction, IDMapOwnPointer> MapType;
    MapType map_;
    WebIDBObjectIDToURLMap transaction_url_map_;
    WebIDBTransactionIDToSizeMap transaction_size_map_;
  };

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  // Only access on WebKit thread.
  scoped_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  scoped_ptr<IndexDispatcherHost> index_dispatcher_host_;
  scoped_ptr<ObjectStoreDispatcherHost> object_store_dispatcher_host_;
  scoped_ptr<CursorDispatcherHost> cursor_dispatcher_host_;
  scoped_ptr<TransactionDispatcherHost> transaction_dispatcher_host_;

  // Used to dispatch messages to the correct view host.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
