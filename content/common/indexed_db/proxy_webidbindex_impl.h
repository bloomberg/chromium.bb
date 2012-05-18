// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBINDEX_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBINDEX_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBIndex.h"

class RendererWebIDBIndexImpl : public WebKit::WebIDBIndex {
 public:
  explicit RendererWebIDBIndexImpl(int32 idb_index_id);
  virtual ~RendererWebIDBIndexImpl();

  // WebKit::WebIDBIndex
  virtual WebKit::WebString name() const;
  virtual WebKit::WebIDBKeyPath keyPath() const;
  virtual bool unique() const;
  virtual bool multiEntry() const;

  virtual void openObjectCursor(const WebKit::WebIDBKeyRange& range,
                                unsigned short direction,
                                WebKit::WebIDBCallbacks* callbacks,
                                const WebKit::WebIDBTransaction& transaction,
                                WebKit::WebExceptionCode& ec);
  virtual void openKeyCursor(const WebKit::WebIDBKeyRange& range,
                             unsigned short direction,
                             WebKit::WebIDBCallbacks* callbacks,
                             const WebKit::WebIDBTransaction& transaction,
                             WebKit::WebExceptionCode& ec);
  virtual void count(const WebKit::WebIDBKeyRange& range,
                     WebKit::WebIDBCallbacks* callbacks,
                     const WebKit::WebIDBTransaction& transaction,
                     WebKit::WebExceptionCode& ec);
  virtual void getObject(const WebKit::WebIDBKeyRange& key_range,
                         WebKit::WebIDBCallbacks* callbacks,
                         const WebKit::WebIDBTransaction& transaction,
                         WebKit::WebExceptionCode& ec);
  virtual void getKey(const WebKit::WebIDBKeyRange& key_range,
                      WebKit::WebIDBCallbacks* callbacks,
                      const WebKit::WebIDBTransaction& transaction,
                      WebKit::WebExceptionCode& ec);

 private:
  int32 idb_index_id_;
};

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBINDEX_IMPL_H_
