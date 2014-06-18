// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_
#define COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "components/leveldb_proto/proto_database.h"

namespace leveldb_proto {
namespace test {

template <typename T>
class FakeDB : public ProtoDatabase<T> {
  typedef base::Callback<void(bool)> Callback;

 public:
  typedef typename base::hash_map<std::string, T> EntryMap;

  explicit FakeDB(EntryMap* db);
  virtual ~FakeDB();

  virtual void Init(const base::FilePath& database_dir,
                    typename ProtoDatabase<T>::InitCallback callback)
      OVERRIDE;

  virtual void UpdateEntries(
      scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
      scoped_ptr<std::vector<std::string> > keys_to_remove,
      typename ProtoDatabase<T>::UpdateCallback callback) OVERRIDE;

  virtual void LoadEntries(typename ProtoDatabase<T>::LoadCallback callback)
      OVERRIDE;
  base::FilePath& GetDirectory();

  void InitCallback(bool success);

  void LoadCallback(bool success);

  void UpdateCallback(bool success);

  static base::FilePath DirectoryForTestDB();

 private:
  static void RunLoadCallback(
      typename ProtoDatabase<T>::LoadCallback callback,
      scoped_ptr<typename std::vector<T> > entries,
      bool success);

  base::FilePath dir_;
  EntryMap* db_;

  Callback init_callback_;
  Callback load_callback_;
  Callback update_callback_;
};

template <typename T>
FakeDB<T>::FakeDB(EntryMap* db)
    : db_(db) {}

template <typename T>
FakeDB<T>::~FakeDB() {}

template <typename T>
void FakeDB<T>::Init(const base::FilePath& database_dir,
                     typename ProtoDatabase<T>::InitCallback callback) {
  dir_ = database_dir;
  init_callback_ = callback;
}

template <typename T>
void FakeDB<T>::UpdateEntries(
    scoped_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    scoped_ptr<std::vector<std::string> > keys_to_remove,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  for (typename ProtoDatabase<T>::KeyEntryVector::iterator it =
           entries_to_save->begin();
       it != entries_to_save->end(); ++it) {
    (*db_)[it->first] = it->second;
  }
  for (std::vector<std::string>::iterator it = keys_to_remove->begin();
       it != keys_to_remove->end(); ++it) {
    (*db_).erase(*it);
  }
  update_callback_ = callback;
}

template <typename T>
void FakeDB<T>::LoadEntries(typename ProtoDatabase<T>::LoadCallback callback) {
  scoped_ptr<std::vector<T> > entries(new std::vector<T>());
  for (typename EntryMap::iterator it = db_->begin(); it != db_->end(); ++it) {
    entries->push_back(it->second);
  }
  load_callback_ =
      base::Bind(RunLoadCallback, callback, base::Passed(&entries));
}

template <typename T>
base::FilePath& FakeDB<T>::GetDirectory() {
  return dir_;
}

template <typename T>
void FakeDB<T>::InitCallback(bool success) {
  init_callback_.Run(success);
  init_callback_.Reset();
}

template <typename T>
void FakeDB<T>::LoadCallback(bool success) {
  load_callback_.Run(success);
  load_callback_.Reset();
}

template <typename T>
void FakeDB<T>::UpdateCallback(bool success) {
  update_callback_.Run(success);
  update_callback_.Reset();
}

// static
template <typename T>
void FakeDB<T>::RunLoadCallback(
    typename ProtoDatabase<T>::LoadCallback callback,
    scoped_ptr<typename std::vector<T> > entries, bool success) {
  callback.Run(success, entries.Pass());
}

// static
template <typename T>
base::FilePath FakeDB<T>::DirectoryForTestDB() {
  return base::FilePath(FILE_PATH_LITERAL("/fake/path"));
}

}  // namespace test
}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_TESTING_FAKE_DB_H_
