// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

// Small class that strictly performs the migration functionality of
// SharedProtoDatabase.
template <typename T>
class MigrationDelegate {
 public:
  using MigrationCallback = base::OnceCallback<void(bool)>;

  MigrationDelegate();

  // Copies the keys/entries of |from| to |to|, and returns a boolean success
  // in |callback|.
  void DoMigration(ProtoDatabase<T>* from,
                   ProtoDatabase<T>* to,
                   MigrationCallback callback);

 private:
  void OnLoadKeysAndEntries(
      MigrationCallback callback,
      ProtoDatabase<T>* to,
      bool success,
      std::unique_ptr<std::map<std::string, T>> keys_entries);
  void OnUpdateEntries(MigrationCallback callback, bool success);

  base::WeakPtrFactory<MigrationDelegate<T>> weak_ptr_factory_;
};

template <typename T>
MigrationDelegate<T>::MigrationDelegate() : weak_ptr_factory_(this) {}

template <typename T>
void MigrationDelegate<T>::DoMigration(ProtoDatabase<T>* from,
                                       ProtoDatabase<T>* to,
                                       MigrationCallback callback) {
  from->LoadKeysAndEntries(
      base::BindOnce(&MigrationDelegate<T>::OnLoadKeysAndEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::Unretained(to)));
}

template <typename T>
void MigrationDelegate<T>::OnLoadKeysAndEntries(
    MigrationCallback callback,
    ProtoDatabase<T>* to,
    bool success,
    std::unique_ptr<std::map<std::string, T>> keys_entries) {
  if (!success) {
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
    current_task_runner->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback), false));
    return;
  }

  // Convert the std::map we got back into a vector of std::pairs to be used
  // with UpdateEntries.
  auto kev = std::make_unique<typename ProtoDatabase<T>::KeyEntryVector>();
  for (auto const& key_entry : *keys_entries)
    kev->push_back(key_entry);

  // Save the entries in |to|.
  to->UpdateEntries(
      std::move(kev), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&MigrationDelegate<T>::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

template <typename T>
void MigrationDelegate<T>::OnUpdateEntries(MigrationCallback callback,
                                           bool success) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  current_task_runner->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  // TODO (thildebr): For additional insurance, verify the entries match,
  // although they should if we got a success from UpdateEntries.
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_MIGRATION_DELEGATE_H_