// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class KeyUtilityClientImpl;

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class SerializedScriptValue;
}

// Class for obtaining IndexedDBKeys from the SerializedScriptValues given
// an IDBKeyPath. This class is a thin singleton wrapper around the
// KeyUtilityClientImpl, which does the real work.
class IndexedDBKeyUtilityClient {
 public:
  // Synchronously obtain the |keys| from |values| for the given |key_path|.
  static void CreateIDBKeysFromSerializedValuesAndKeyPath(
      const std::vector<content::SerializedScriptValue>& values,
      const content::IndexedDBKeyPath& key_path,
      std::vector<content::IndexedDBKey>* keys);

  // Synchronously inject |key| into |value| using |key_path|. Returns the new
  // value.
  static content::SerializedScriptValue InjectIDBKeyIntoSerializedValue(
      const content::IndexedDBKey& key,
      const content::SerializedScriptValue& value,
      const content::IndexedDBKeyPath& key_path);

  // Shut down the underlying implementation. Must be called on the IO thread.
  static void Shutdown();

 private:
  friend struct base::DefaultLazyInstanceTraits<IndexedDBKeyUtilityClient>;
  IndexedDBKeyUtilityClient();
  ~IndexedDBKeyUtilityClient();

  bool is_shutdown_;

  // The real client; laziliy instantiated.
  scoped_refptr<KeyUtilityClientImpl> impl_;
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_KEY_UTILITY_CLIENT_H_
