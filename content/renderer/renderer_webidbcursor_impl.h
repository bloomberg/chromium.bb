// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_

#include <deque>

#include "base/basictypes.h"
#include "content/common/indexed_db_key.h"
#include "content/public/common/serialized_script_value.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"

class RendererWebIDBCursorImpl : public WebKit::WebIDBCursor {
 public:
  RendererWebIDBCursorImpl(int32 idb_cursor_id);
  virtual ~RendererWebIDBCursorImpl();

  virtual unsigned short direction() const;
  virtual WebKit::WebIDBKey key() const;
  virtual WebKit::WebIDBKey primaryKey() const;
  virtual WebKit::WebSerializedScriptValue value() const;
  virtual void update(const WebKit::WebSerializedScriptValue& value,
                      WebKit::WebIDBCallbacks* callback,
                      WebKit::WebExceptionCode& ec);
  virtual void continueFunction(const WebKit::WebIDBKey& key,
                                WebKit::WebIDBCallbacks* callback,
                                WebKit::WebExceptionCode& ec);
  virtual void deleteFunction(WebKit::WebIDBCallbacks* callback,
                              WebKit::WebExceptionCode& ec);
  virtual void postSuccessHandlerCallback();

  void SetKeyAndValue(const IndexedDBKey& key, const IndexedDBKey& primary_key,
                      const content::SerializedScriptValue& value);
  void SetPrefetchData(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<content::SerializedScriptValue>& values);

  void CachedContinue(WebKit::WebIDBCallbacks* callbacks);
  void ResetPrefetchCache();

 private:
  int32 idb_cursor_id_;
  IndexedDBKey key_;
  IndexedDBKey primary_key_;
  content::SerializedScriptValue value_;

  // Prefetch cache.
  std::deque<IndexedDBKey> prefetch_keys_;
  std::deque<IndexedDBKey> prefetch_primary_keys_;
  std::deque<content::SerializedScriptValue> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  enum { kPrefetchContinueThreshold = 2 };
  enum { kMinPrefetchAmount = 5 };
  enum { kMaxPrefetchAmount = 100 };
};

#endif  // CONTENT_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_
