// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace leveldb_proto {

// Interface for classes providing persistent storage of Protocol Buffer
// entries (T must be a Proto type extending MessageLite).
template <typename T>
class ProtoDatabase {
 public:
  typedef base::Callback<void(bool success)> InitCallback;
  typedef base::Callback<void(bool success)> UpdateCallback;
  typedef base::Callback<void(bool success, scoped_ptr<std::vector<T> >)>
      LoadCallback;
  // A list of key-value (string, T) tuples.
  typedef std::vector<std::pair<std::string, T> > KeyEntryVector;

  virtual ~ProtoDatabase() {}

  // Asynchronously initializes the object. |callback| will be invoked on the
  // calling thread when complete.
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete.
  virtual void UpdateEntries(
      scoped_ptr<KeyEntryVector> entries_to_save,
      scoped_ptr<std::vector<std::string> > keys_to_remove,
      UpdateCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(LoadCallback callback) = 0;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_
