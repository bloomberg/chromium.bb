// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_TESTING_TEST_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_TESTING_TEST_PROTO_DATABASE_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace leveldb_proto {

// TestProtoDatabaseImpl is a wrapper of ProtoDataBaseImpl, with the additional
// functionality of counting the number of times the database is hit, which can
// be accessed with GetNumberOfDatabaseCalls().
template <typename T>
class TestProtoDatabaseImpl : public ProtoDatabaseImpl<T> {
 public:
  explicit TestProtoDatabaseImpl(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  void UpdateEntries(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      std::unique_ptr<KeyVector> keys_to_remove,
      typename ProtoDatabase<T>::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const LevelDB::KeyFilter& delete_key_filter,
      typename ProtoDatabase<T>::UpdateCallback callback) override;
  void LoadEntries(typename ProtoDatabase<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      typename ProtoDatabase<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const LevelDB::KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename ProtoDatabase<T>::LoadCallback callback) override;
  void LoadKeys(typename ProtoDatabase<T>::LoadKeysCallback callback) override;
  void GetEntry(const std::string& key,
                typename ProtoDatabase<T>::GetCallback callback) override;
  void Destroy(typename ProtoDatabase<T>::DestroyCallback callback) override;

  int number_of_db_calls();

 private:
  int IncrementDatabaseCalls();

  // The number of times the database is called.
  int db_calls_ = 0;
};

template <typename T>
TestProtoDatabaseImpl<T>::TestProtoDatabaseImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : ProtoDatabaseImpl<T>(task_runner) {}

template <typename T>
void TestProtoDatabaseImpl<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  IncrementDatabaseCalls();
  leveldb_proto::ProtoDatabaseImpl<T>::UpdateEntries(std::move(entries_to_save),
                                                     std::move(keys_to_remove),
                                                     std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const LevelDB::KeyFilter& delete_key_filter,
    typename ProtoDatabase<T>::UpdateCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save), delete_key_filter, std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::LoadEntries(
    typename ProtoDatabase<T>::LoadCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::LoadEntries(std::move(callback));
}

template <typename T>
void leveldb_proto::TestProtoDatabaseImpl<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    typename ProtoDatabase<T>::LoadCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::LoadEntriesWithFilter(key_filter, std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::LoadEntriesWithFilter(
    const LevelDB::KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename ProtoDatabase<T>::LoadCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::LoadEntriesWithFilter(
      key_filter, options, target_prefix, std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::LoadKeys(
    typename ProtoDatabase<T>::LoadKeysCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::LoadKeys(std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::GetEntry(
    const std::string& key,
    typename ProtoDatabase<T>::GetCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::GetEntry(key, std::move(callback));
}

template <typename T>
void TestProtoDatabaseImpl<T>::Destroy(
    typename ProtoDatabase<T>::DestroyCallback callback) {
  IncrementDatabaseCalls();
  ProtoDatabaseImpl<T>::Destroy(std::move(callback));
}

template <typename T>
int TestProtoDatabaseImpl<T>::number_of_db_calls() {
  return db_calls_;
}

template <typename T>
int TestProtoDatabaseImpl<T>::IncrementDatabaseCalls() {
  return ++db_calls_;
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_TESTING_TEST_PROTO_DATABASE_IMPL_H_
