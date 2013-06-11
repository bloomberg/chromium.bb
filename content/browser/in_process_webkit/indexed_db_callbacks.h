// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/public/platform/WebIDBDatabase.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {
class WebIDBCursorImpl;
class WebIDBDatabaseImpl;
struct IndexedDBDatabaseMetadata;

class IndexedDBCallbacksBase {
 public:
  virtual ~IndexedDBCallbacksBase();

  virtual void onError(const WebKit::WebIDBDatabaseError& error);
  virtual void onBlocked(long long old_version);

  // implemented by subclasses, but need to be called later
  virtual void onSuccess(const std::vector<string16>& value);
  virtual void onSuccess(WebIDBDatabaseImpl* idb_object,
                         const IndexedDBDatabaseMetadata& metadata);
  virtual void onUpgradeNeeded(long long old_version,
                               WebIDBDatabaseImpl* database,
                               const IndexedDBDatabaseMetadata&);
  virtual void onSuccess(WebIDBCursorImpl* idb_object,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primaryKey,
                         std::vector<char>* value);
  virtual void onSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primaryKey,
                         std::vector<char>* value);
  virtual void onSuccess(std::vector<char>* value);
  virtual void onSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primaryKeys,
      const std::vector<std::vector<char> >& values);
  virtual void onSuccess(const IndexedDBKey& value);
  virtual void onSuccess(std::vector<char>* value,
                         const IndexedDBKey& key,
                         const IndexedDBKeyPath& keyPath);
  virtual void onSuccess(long long value);
  virtual void onSuccess();

 protected:
  IndexedDBCallbacksBase(IndexedDBDispatcherHost* dispatcher_host,
                         int32 ipc_thread_id,
                         int32 ipc_callbacks_id);
  IndexedDBDispatcherHost* dispatcher_host() const {
    return dispatcher_host_.get();
  }
  int32 ipc_thread_id() const { return ipc_thread_id_; }
  int32 ipc_callbacks_id() const { return ipc_callbacks_id_; }

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32 ipc_callbacks_id_;
  int32 ipc_thread_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacksBase);
};

// TODO(dgrogan): Remove this class and change the remaining specializations
// into subclasses of IndexedDBCallbacksBase.
template <class WebObjectType>
class IndexedDBCallbacks : public IndexedDBCallbacksBase {
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

class IndexedDBCallbacksDatabase : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacksDatabase(IndexedDBDispatcherHost* dispatcher_host,
                             int32 ipc_thread_id,
                             int32 ipc_callbacks_id,
                             int32 ipc_database_callbacks_id,
                             int64 host_transaction_id,
                             const GURL& origin_url);

  virtual void onSuccess(WebIDBDatabaseImpl* idb_object,
                         const IndexedDBDatabaseMetadata& metadata) OVERRIDE;
  virtual void onUpgradeNeeded(long long old_version,
                               WebIDBDatabaseImpl* database,
                               const IndexedDBDatabaseMetadata&) OVERRIDE;

 private:
  int64 host_transaction_id_;
  GURL origin_url_;
  int32 ipc_database_id_;
  int32 ipc_database_callbacks_id_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacksDatabase);
};

// WebIDBCursorImpl uses:
// * onSuccess(WebIDBCursorImpl*, WebIDBKey, WebIDBKey, WebData)
//   when an openCursor()/openKeyCursor() call has succeeded,
// * onSuccess(WebIDBKey, WebIDBKey, WebData)
//   when an advance()/continue() call has succeeded, or
// * onSuccess()
//   to indicate it does not contain any data, i.e., there is no key within
//   the key range, or it has reached the end.
template <>
class IndexedDBCallbacks<WebIDBCursorImpl> : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id,
                     int32 ipc_cursor_id)
      : IndexedDBCallbacksBase(dispatcher_host,
                               ipc_thread_id,
                               ipc_callbacks_id),
        ipc_cursor_id_(ipc_cursor_id) {}

  virtual void onSuccess(WebIDBCursorImpl* idb_object,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primaryKey,
                         std::vector<char>* value);
  virtual void onSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primaryKey,
                         std::vector<char>* value);
  virtual void onSuccess(std::vector<char>* value);
  virtual void onSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primaryKeys,
      const std::vector<std::vector<char> >& values);

 private:
  // The id of the cursor this callback concerns, or -1 if the cursor
  // does not exist yet.
  int32 ipc_cursor_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebIDBKey is implemented in WebKit as opposed to being an interface Chromium
// implements.  Thus we pass a const ___& version and thus we need this
// specialization.
template <>
class IndexedDBCallbacks<IndexedDBKey> : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id)
      : IndexedDBCallbacksBase(dispatcher_host,
                               ipc_thread_id,
                               ipc_callbacks_id) {}

  virtual void onSuccess(const IndexedDBKey& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

template <>
class IndexedDBCallbacks<
    std::vector<string16> > : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id)
      : IndexedDBCallbacksBase(dispatcher_host,
                               ipc_thread_id,
                               ipc_callbacks_id) {}

  virtual void onSuccess(const std::vector<string16>& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebData is implemented in WebKit as opposed to being an interface
// Chromium implements.  Thus we pass a const ___& version and thus we
// need this specialization.
template <>
class IndexedDBCallbacks<std::vector<char> > : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id)
      : IndexedDBCallbacksBase(dispatcher_host,
                               ipc_thread_id,
                               ipc_callbacks_id) {}

  virtual void onSuccess(std::vector<char>* value);
  virtual void onSuccess(std::vector<char>* value,
                         const IndexedDBKey& key,
                         const IndexedDBKeyPath& keyPath);
  virtual void onSuccess(long long value);
  virtual void onSuccess();
  virtual void onSuccess(const IndexedDBKey& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
