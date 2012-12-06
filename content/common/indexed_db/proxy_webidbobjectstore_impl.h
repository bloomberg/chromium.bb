// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBOBJECTSTORE_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBOBJECTSTORE_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"

namespace WebKit {
class WebIDBCallbacks;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyRange;
class WebString;
}

namespace content {

class RendererWebIDBObjectStoreImpl : public WebKit::WebIDBObjectStore {
 public:
  explicit RendererWebIDBObjectStoreImpl(int32 ipc_object_store_id);
  virtual ~RendererWebIDBObjectStoreImpl();

  // TODO(alecflett): Remove this when it is removed from webkit:
  // https://bugs.webkit.org/show_bug.cgi?id=98085
  static const long long AutogenerateIndexId = -1;

  // WebKit::WebIDBObjectStore
  virtual void get(const WebKit::WebIDBKeyRange& key_range,
                   WebKit::WebIDBCallbacks* callbacks,
                   const WebKit::WebIDBTransaction& transaction,
                   WebKit::WebExceptionCode& ec);
  virtual void put(
      const WebKit::WebSerializedScriptValue&,
      const WebKit::WebIDBKey&,
      PutMode,
      WebKit::WebIDBCallbacks*,
      const WebKit::WebIDBTransaction&,
      const WebKit::WebVector<long long>&,
      const WebKit::WebVector<WebKit::WebIDBObjectStore::WebIndexKeys>&);
  virtual void setIndexKeys(const WebKit::WebIDBKey&,
                            const WebKit::WebVector<long long>&,
                            const WebKit::WebVector<WebIndexKeys>&,
                            const WebKit::WebIDBTransaction&);
  virtual void setIndexesReady(const WebKit::WebVector<long long>&,
                               const WebKit::WebIDBTransaction&);
  virtual void deleteFunction(const WebKit::WebIDBKeyRange& key_range,
                              WebKit::WebIDBCallbacks* callbacks,
                              const WebKit::WebIDBTransaction& transaction,
                              WebKit::WebExceptionCode& ec);
  virtual void clear(WebKit::WebIDBCallbacks* callbacks,
                     const WebKit::WebIDBTransaction& transaction,
                     WebKit::WebExceptionCode& ec);

  virtual WebKit::WebIDBIndex* createIndex(
      long long index_id,
      const WebKit::WebString& name,
      const WebKit::WebIDBKeyPath& key_path,
      bool unique,
      bool multi_entry,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode& ec);

  // Transfers ownership of the WebIDBIndex to the caller.
  virtual WebKit::WebIDBIndex* index(long long object_store_id);
  virtual void deleteIndex(long long index_id,
                           const WebKit::WebIDBTransaction& transaction,
                           WebKit::WebExceptionCode& ec);

  virtual void openCursor(const WebKit::WebIDBKeyRange&,
                          WebKit::WebIDBCursor::Direction direction,
                          WebKit::WebIDBCallbacks*,
                          WebKit::WebIDBTransaction::TaskType,
                          const WebKit::WebIDBTransaction&,
                          WebKit::WebExceptionCode&);

  virtual void count(const WebKit::WebIDBKeyRange& idb_key_range,
                     WebKit::WebIDBCallbacks* callbacks,
                     const WebKit::WebIDBTransaction& transaction,
                     WebKit::WebExceptionCode& ec);

 private:
  int32 ipc_object_store_id_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBOBJECTSTORE_IMPL_H_
