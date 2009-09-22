// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_

#include "base/string16.h"
#include "base/hash_tables.h"
#include "chrome/common/dom_storage_type.h"

class DOMStorageContext;
class FilePath;
class StorageArea;

namespace WebKit {
class WebStorageNamespace;
}

// Only to be used on the WebKit thread.
class StorageNamespace {
 public:
  static StorageNamespace* CreateLocalStorageNamespace(
      DOMStorageContext* dom_storage_context, const FilePath& data_dir_path);
  static StorageNamespace* CreateSessionStorageNamespace(
      DOMStorageContext* dom_storage_context);

  ~StorageNamespace();

  StorageArea* GetStorageArea(const string16& origin);
  StorageNamespace* Copy();
  void Close();

  int64 id() const { return id_; }

 private:
  // Called by the static factory methods above.
  StorageNamespace(DOMStorageContext* dom_storage_context,
                   WebKit::WebStorageNamespace* web_storage_namespace,
                   int64 id, DOMStorageType storage_type);

  // All the storage areas we own.
  typedef base::hash_map<string16, StorageArea*> OriginToStorageAreaMap;
  OriginToStorageAreaMap origin_to_storage_area_;

  // The DOMStorageContext that owns us.
  DOMStorageContext* dom_storage_context_;

  // The WebKit storage namespace we manage.
  WebKit::WebStorageNamespace* storage_namespace_;

  // Our id.  Unique to our parent WebKitContext class.
  int64 id_;

  // SessionStorage vs. LocalStorage.
  const DOMStorageType storage_type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StorageNamespace);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_
