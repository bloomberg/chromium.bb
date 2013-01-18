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
struct IndexedDBDatabaseMetadata;
struct IndexedDBHostMsg_DatabaseCount_Params;
struct IndexedDBHostMsg_DatabaseCreateIndex_Params;
struct IndexedDBHostMsg_DatabaseCreateObjectStoreOld_Params;
struct IndexedDBHostMsg_DatabaseCreateObjectStore_Params;
struct IndexedDBHostMsg_DatabaseCreateTransaction_Params;
struct IndexedDBHostMsg_DatabaseDeleteRange_Params;
struct IndexedDBHostMsg_DatabaseGet_Params;
struct IndexedDBHostMsg_DatabaseOpenCursor_Params;
struct IndexedDBHostMsg_DatabasePut_Params;
struct IndexedDBHostMsg_DatabaseSetIndexKeys_Params;
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
class IndexedDBContextImpl;
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
class SerializedScriptValue;

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost : public BrowserMessageFilter {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(int ipc_process_id,
                          IndexedDBContextImpl* indexed_db_context);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void TransactionComplete(int32 ipc_transaction_id);
  void TransactionIdComplete(int64 host_transaction_id);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* Context() { return indexed_db_context_; }

  // The various IndexedDBCallbacks children call these methods to add the
  // results into the applicable map.  See below for more details.
  int32 Add(WebKit::WebIDBCursor* idb_cursor);
  int32 Add(WebKit::WebIDBDatabase* idb_database,
            int32 ipc_thread_id,
            const GURL& origin_url);
  int32 Add(WebKit::WebIDBTransaction* idb_transaction,
            int32 ipc_thread_id,
            const GURL& origin_url);
  int32 Add(WebKit::WebDOMStringList* domStringList);

  void RegisterTransactionId(int64 host_transaction_id, const GURL& origin_url);

  WebKit::WebIDBCursor* GetCursorFromId(int32 ipc_cursor_id);

  int64 HostTransactionId(int64 transaction_id);
  int64 RendererTransactionId(int64 host_transaction_id);

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
    IDMap<ReturnType, IDMapOwnPointer>* map, int32 ipc_return_object_id);

  template <typename ObjectType>
  void DestroyObject(IDMap<ObjectType, IDMapOwnPointer>* map,
                     int32 ipc_object_id);

  // Used in nested classes.
  typedef std::map<int32, GURL> WebIDBObjectIDToURLMap;
  typedef std::map<int32, uint64> WebIDBTransactionIPCIDToSizeMap;

  typedef std::map<int64, GURL> TransactionIDToURLMap;
  typedef std::map<int64, uint64> WebIDBTransactionIDToSizeMap;

  class DatabaseDispatcherHost {
   public:
    explicit DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnMetadata(int32 ipc_database_id,
                    IndexedDBDatabaseMetadata* metadata);
    void OnCreateObjectStore(
        const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params);
    void OnDeleteObjectStore(int32 ipc_database_id,
                             int64 transaction_id,
                             int64 object_store_id);
    void OnCreateTransactionOld(int32 ipc_thread_id,
                                int32 ipc_database_id,
                                int64 transaction_id,
                                const std::vector<int64>& scope,
                                int32 mode,
                                int32* ipc_transaction_id);
    void OnCreateTransaction(
        const IndexedDBHostMsg_DatabaseCreateTransaction_Params&);
    void OnOpen(int32 ipc_database_id, int32 ipc_thread_id,
                int32 ipc_response_id);
    void OnClose(int32 ipc_database_id);
    void OnDestroyed(int32 ipc_database_id);

    void OnGet(const IndexedDBHostMsg_DatabaseGet_Params& params);
    void OnPut(const IndexedDBHostMsg_DatabasePut_Params& params);
    void OnSetIndexKeys(
        const IndexedDBHostMsg_DatabaseSetIndexKeys_Params& params);
    void OnSetIndexesReady(
        int32 ipc_database_id,
        int64 transaction_id,
        int64 object_store_id,
        const std::vector<int64>& ids);
    void OnOpenCursor(
        const IndexedDBHostMsg_DatabaseOpenCursor_Params& params);
    void OnCount(const IndexedDBHostMsg_DatabaseCount_Params& params);
    void OnDeleteRange(
        const IndexedDBHostMsg_DatabaseDeleteRange_Params& params);
    void OnClear(int32 ipc_thread_id,
                 int32 ipc_response_id,
                 int32 ipc_database_id,
                 int64 transaction_id,
                 int64 object_store_id);
    void OnCreateIndex(
        const IndexedDBHostMsg_DatabaseCreateIndex_Params& params);
    void OnDeleteIndex(int32 ipc_database_id,
                       int64 transaction_id,
                       int64 object_store_id,
                       int64 index_id);

    void OnAbort(int32 ipc_database_id, int64 transaction_id);
    void OnCommit(int32 ipc_database_id, int64 transaction_id);
    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBDatabase, IDMapOwnPointer> map_;
    WebIDBObjectIDToURLMap database_url_map_;
    WebIDBTransactionIDToSizeMap transaction_size_map_;
    TransactionIDToURLMap transaction_url_map_;
  };

  class CursorDispatcherHost {
   public:
    explicit CursorDispatcherHost(IndexedDBDispatcherHost* parent);
    ~CursorDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnUpdate(int32 ipc_object_store_id,
                  int32 ipc_thread_id,
                  int32 ipc_response_id,
                  const SerializedScriptValue& value);
    void OnAdvance(int32 ipc_object_store_id,
                   int32 ipc_thread_id,
                   int32 ipc_response_id,
                   unsigned long count);
    void OnContinue(int32 ipc_object_store_id,
                    int32 ipc_thread_id,
                    int32 ipc_response_id,
                    const IndexedDBKey& key);
    void OnPrefetch(int32 ipc_cursor_id,
                    int32 ipc_thread_id,
                    int32 ipc_response_id,
                    int n);
    void OnPrefetchReset(int32 ipc_cursor_id, int used_prefetches,
                         int unused_prefetches);
    void OnDelete(int32 ipc_object_store_id,
                  int32 ipc_thread_id,
                  int32 ipc_response_id);
    void OnDestroyed(int32 ipc_cursor_id);

    IndexedDBDispatcherHost* parent_;
    IDMap<WebKit::WebIDBCursor, IDMapOwnPointer> map_;
  };

  class TransactionDispatcherHost {
   public:
    explicit TransactionDispatcherHost(IndexedDBDispatcherHost* parent);
    ~TransactionDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool *msg_is_ok);
    void Send(IPC::Message* message);

    void OnCommit(int32 ipc_transaction_id);
    void OnAbort(int32 ipc_transaction_id);
    void OnDestroyed(int32 ipc_transaction_id);

    IndexedDBDispatcherHost* parent_;
    typedef IDMap<WebKit::WebIDBTransaction, IDMapOwnPointer> MapType;
    MapType map_;
    WebIDBObjectIDToURLMap old_transaction_url_map_;
    WebIDBTransactionIPCIDToSizeMap transaction_ipc_size_map_;
  };

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  // Only access on WebKit thread.
  scoped_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  scoped_ptr<CursorDispatcherHost> cursor_dispatcher_host_;
  scoped_ptr<TransactionDispatcherHost> transaction_dispatcher_host_;

  // Used to dispatch messages to the correct view host.
  int ipc_process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DISPATCHER_HOST_H_
