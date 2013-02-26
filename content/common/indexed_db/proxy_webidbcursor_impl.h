// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBCURSOR_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBCURSOR_IMPL_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"

namespace content {

class CONTENT_EXPORT RendererWebIDBCursorImpl
    : NON_EXPORTED_BASE(public WebKit::WebIDBCursor) {
 public:
  explicit RendererWebIDBCursorImpl(int32 ipc_cursor_id);
  virtual ~RendererWebIDBCursorImpl();

  virtual void advance(unsigned long count,
                       WebKit::WebIDBCallbacks* callback,
                       WebKit::WebExceptionCode& ec);
  virtual void continueFunction(const WebKit::WebIDBKey& key,
                                WebKit::WebIDBCallbacks* callback,
                                WebKit::WebExceptionCode& ec);
  virtual void deleteFunction(WebKit::WebIDBCallbacks* callback,
                              WebKit::WebExceptionCode& ec);
  virtual void postSuccessHandlerCallback();

  void SetPrefetchData(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<WebKit::WebData>& values);

  void CachedContinue(WebKit::WebIDBCallbacks* callbacks);
  void ResetPrefetchCache();

 private:
  FRIEND_TEST_ALL_PREFIXES(RendererWebIDBCursorImplTest, PrefetchTest);

  int32 ipc_cursor_id_;

  // Prefetch cache.
  std::deque<IndexedDBKey> prefetch_keys_;
  std::deque<IndexedDBKey> prefetch_primary_keys_;
  std::deque<WebKit::WebData> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  enum { kInvalidCursorId = -1 };
  enum { kPrefetchContinueThreshold = 2 };
  enum { kMinPrefetchAmount = 5 };
  enum { kMaxPrefetchAmount = 100 };
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBCURSOR_IMPL_H_
