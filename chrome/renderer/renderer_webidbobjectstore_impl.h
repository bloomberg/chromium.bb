// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"

namespace WebKit {
class WebFrame;
class WebIDBCallbacks;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyRange;
class WebString;
}

class RendererWebIDBObjectStoreImpl : public WebKit::WebIDBObjectStore {
 public:
  explicit RendererWebIDBObjectStoreImpl(int32 idb_object_store_id);
  ~RendererWebIDBObjectStoreImpl();

  // WebKit::WebIDBObjectStore
  WebKit::WebString name() const;
  WebKit::WebString keyPath() const;
  WebKit::WebDOMStringList indexNames() const;

  void get(const WebKit::WebIDBKey& key,
           WebKit::WebIDBCallbacks* callbacks,
           const WebKit::WebIDBTransaction& transaction,
           WebKit::WebExceptionCode& ec);
  void put(const WebKit::WebSerializedScriptValue& value,
           const WebKit::WebIDBKey& key,
           bool add_only,
           WebKit::WebIDBCallbacks* callbacks,
           const WebKit::WebIDBTransaction& transaction,
           WebKit::WebExceptionCode& ec);
  void remove(const WebKit::WebIDBKey& key,
              WebKit::WebIDBCallbacks* callbacks,
              const WebKit::WebIDBTransaction& transaction,
              WebKit::WebExceptionCode& ec);

  WebKit::WebIDBIndex* createIndex(
      const WebKit::WebString& name,
      const WebKit::WebString& key_path,
      bool unique,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode& ec);
  // Transfers ownership of the WebIDBIndex to the caller.
  WebKit::WebIDBIndex* index(const WebKit::WebString& name,
                             WebKit::WebExceptionCode& ec);
  void deleteIndex(const WebKit::WebString& name,
                   const WebKit::WebIDBTransaction& transaction,
                   WebKit::WebExceptionCode& ec);

  void openCursor(const WebKit::WebIDBKeyRange& idb_key_range,
                  unsigned short direction,
                  WebKit::WebIDBCallbacks* callbacks,
                  const WebKit::WebIDBTransaction& transaction,
                  WebKit::WebExceptionCode& ec);
 private:
  int32 idb_object_store_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBOBJECTSTORE_IMPL_H_
