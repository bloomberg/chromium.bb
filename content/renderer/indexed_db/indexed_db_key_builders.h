// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_
#define CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"

namespace blink {

class IndexedDBKeyRange;
class WebIDBKeyPath;
class WebIDBKeyRange;

}  // namespace blink

namespace content {

class CONTENT_EXPORT IndexedDBKeyBuilder {
 public:
  static blink::IndexedDBKey Build(blink::WebIDBKeyView key);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyBuilder);
};

class CONTENT_EXPORT WebIDBKeyBuilder {
 public:
  static blink::WebIDBKey Build(const blink::IndexedDBKey& key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyBuilder);
};

class CONTENT_EXPORT IndexedDBKeyRangeBuilder {
 public:
  static blink::IndexedDBKeyRange Build(const blink::WebIDBKeyRange& key_range);

  // Builds a point range (containing a single key).
  static blink::IndexedDBKeyRange Build(blink::WebIDBKeyView key);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyRangeBuilder);
};

class CONTENT_EXPORT WebIDBKeyRangeBuilder {
 public:
  static blink::WebIDBKeyRange Build(const blink::IndexedDBKeyRange& key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyRangeBuilder);
};

class CONTENT_EXPORT IndexedDBKeyPathBuilder {
 public:
  static blink::IndexedDBKeyPath Build(const blink::WebIDBKeyPath& key_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyPathBuilder);
};

class CONTENT_EXPORT WebIDBKeyPathBuilder {
 public:
  static blink::WebIDBKeyPath Build(const blink::IndexedDBKeyPath& key_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyPathBuilder);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_
