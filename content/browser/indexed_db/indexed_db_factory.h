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
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/common/content_export.h"

namespace content {

class IndexedDBBackingStore;

class CONTENT_EXPORT IndexedDBFactory
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBFactory>) {
 public:
  IndexedDBFactory();

  // Notifications from weak pointers.
  void RemoveIDBDatabaseBackend(
      const IndexedDBDatabase::Identifier& unique_identifier);

  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                        const std::string& origin_identifier,
                        const base::FilePath& data_directory);
  void Open(const string16& name,
            int64 version,
            int64 transaction_id,
            scoped_refptr<IndexedDBCallbacks> callbacks,
            scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
            const std::string& origin_identifier,
            const base::FilePath& data_directory);

  void DeleteDatabase(const string16& name,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const std::string& origin_identifier,
                      const base::FilePath& data_directory);

  // Iterates over all databases; for diagnostics only.
  std::vector<IndexedDBDatabase*> GetOpenDatabasesForOrigin(
      const std::string& origin_identifier) const;

 protected:
  friend class base::RefCounted<IndexedDBFactory>;

  virtual ~IndexedDBFactory();

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const std::string& origin_identifier,
      const base::FilePath& data_directory,
      WebKit::WebIDBCallbacks::DataLoss* data_loss);

 private:
  typedef std::map<IndexedDBDatabase::Identifier,
                   scoped_refptr<IndexedDBDatabase> > IndexedDBDatabaseMap;
  IndexedDBDatabaseMap database_backend_map_;

  typedef std::map<std::string, base::WeakPtr<IndexedDBBackingStore> >
      IndexedDBBackingStoreMap;
  IndexedDBBackingStoreMap backing_store_map_;

  std::set<scoped_refptr<IndexedDBBackingStore> > session_only_backing_stores_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
