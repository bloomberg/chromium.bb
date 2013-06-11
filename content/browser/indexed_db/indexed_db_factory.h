// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/common/content_export.h"

namespace content {

class IndexedDBBackingStore;
class IndexedDBDatabase;

class CONTENT_EXPORT IndexedDBFactory
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBFactory>) {
 public:
  static scoped_refptr<IndexedDBFactory> Create() {
    return make_scoped_refptr(new IndexedDBFactory());
  }

  // Notifications from weak pointers.
  void RemoveIDBDatabaseBackend(const string16& unique_identifier);

  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
                        const string16& database_identifier,
                        const base::FilePath& data_directory);
  void Open(const string16& name,
            int64 version,
            int64 transaction_id,
            scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
            scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
            const string16& database_identifier,
            const base::FilePath& data_directory);

  void DeleteDatabase(const string16& name,
                      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
                      const string16& database_identifier,
                      const base::FilePath& data_directory);

 protected:
  friend class base::RefCounted<IndexedDBFactory>;

  IndexedDBFactory();
  virtual ~IndexedDBFactory();

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const string16& database_identifier,
      const base::FilePath& data_directory);

 private:
  typedef std::map<string16, scoped_refptr<IndexedDBDatabase> >
      IndexedDBDatabaseMap;
  IndexedDBDatabaseMap database_backend_map_;

  typedef std::map<string16, base::WeakPtr<IndexedDBBackingStore> >
      IndexedDBBackingStoreMap;
  IndexedDBBackingStoreMap backing_store_map_;

  std::set<scoped_refptr<IndexedDBBackingStore> > session_only_backing_stores_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
