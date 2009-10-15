// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_

#include "base/string16.h"
#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "chrome/common/dom_storage_type.h"
#include "webkit/api/public/WebString.h"

class DOMStorageContext;
class FilePath;
class StorageArea;

namespace WebKit {
class WebStorageArea;
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
  void PurgeMemory();

  const DOMStorageContext* dom_storage_context() const {
    return dom_storage_context_;
  }
  int64 id() const { return id_; }
  DOMStorageType dom_storage_type() const { return dom_storage_type_; }

  // Creates a WebStorageArea for the given origin.  This should only be called
  // by an owned StorageArea.
  WebKit::WebStorageArea* CreateWebStorageArea(const string16& origin);

 private:
  // Called by the static factory methods above.
  StorageNamespace(DOMStorageContext* dom_storage_context,
                   int64 id,
                   const WebKit::WebString& data_dir_path,
                   DOMStorageType storage_type);

  // Creates the underlying WebStorageNamespace on demand.
  void CreateWebStorageNamespaceIfNecessary();

  // All the storage areas we own.
  typedef base::hash_map<string16, StorageArea*> OriginToStorageAreaMap;
  OriginToStorageAreaMap origin_to_storage_area_;

  // The DOMStorageContext that owns us.
  DOMStorageContext* dom_storage_context_;

  // The WebKit storage namespace we manage.
  scoped_ptr<WebKit::WebStorageNamespace> storage_namespace_;

  // Our id.  Unique to our parent WebKitContext class.
  int64 id_;

  // The path used to create us, so we can recreate our WebStorageNamespace on
  // demand.
  WebKit::WebString data_dir_path_;

  // SessionStorage vs. LocalStorage.
  const DOMStorageType dom_storage_type_;

  // The quota for each storage area.  Suggested by the spec.
  static const unsigned kLocalStorageQuota = 5 * 1024 * 1024;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StorageNamespace);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_STORAGE_NAMESPACE_H_
