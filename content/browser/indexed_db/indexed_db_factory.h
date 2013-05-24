// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"

namespace content {

class IndexedDBCallbacksWrapper;
class IndexedDBDatabase;
class IndexedDBDatabaseCallbacksWrapper;

class IndexedDBFactory : public base::RefCounted<IndexedDBFactory> {
 public:
  static scoped_refptr<IndexedDBFactory> Create();

  virtual void GetDatabaseNames(
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
      const string16& database_identifier,
      const string16& data_dir) = 0;
  virtual void Open(
      const string16& name,
      int64 version,
      int64 transaction_id,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks,
      const string16& database_identifier,
      const string16& data_dir) = 0;
  virtual void DeleteDatabase(
      const string16& name,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
      const string16& database_identifier,
      const string16& data_dir) = 0;

 protected:
  virtual ~IndexedDBFactory() {}
  friend class base::RefCounted<IndexedDBFactory>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
