// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_

#include <queue>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/shared_proto_database_client.h"

namespace leveldb_proto {

template <typename T>
void GetClientInitCallback(
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
        callback,
    std::unique_ptr<SharedProtoDatabaseClient<T>> client,
    Enums::InitStatus status);

// Controls a single LevelDB database to be used by many clients, and provides
// a way to get SharedProtoDatabaseClients that allow shared access to the
// underlying single database.
class SharedProtoDatabase
    : public base::RefCountedThreadSafe<SharedProtoDatabase> {
 public:
  void GetDatabaseInitStatusAsync(Callbacks::InitStatusCallback callback);

  // Always returns a SharedProtoDatabaseClient pointer, but that should ONLY
  // be used if the callback returns success.
  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClient(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      Callbacks::InitStatusCallback callback);

  // A version of GetClient that returns the client in a callback instead of
  // giving back a client instance immediately.
  template <typename T>
  void GetClientAsync(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
          callback);

 private:
  friend class base::RefCountedThreadSafe<SharedProtoDatabase>;
  friend class ProtoDatabaseProvider;

  friend class ProtoDatabaseWrapperTest;
  friend class SharedProtoDatabaseTest;
  friend class SharedProtoDatabaseClientTest;

  enum InitState {
    kNone,
    kInProgress,
    kSuccess,
    kFailure,
  };

  // Private since we only want to create a singleton of it.
  SharedProtoDatabase(
      const std::string& client_name,
      const base::FilePath& db_dir);

  virtual ~SharedProtoDatabase();

  void ProcessInitRequests(Enums::InitStatus status);

  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClientInternal(
      const std::string& client_namespace,
      const std::string& type_prefix);

  // |callback_task_runner| should be the same sequence that Init was called
  // from.
  void Init(bool create_if_missing,
            Callbacks::InitStatusCallback callback,
            scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void OnDatabaseInit(
      Callbacks::InitStatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      Enums::InitStatus status);
  void RunInitCallback(Callbacks::InitStatusCallback callback);

  LevelDB* GetLevelDBForTesting() const;

  SEQUENCE_CHECKER(on_task_runner_);

  InitState init_state_ = InitState::kNone;

  // This TaskRunner is used to properly sequence Init calls and checks for the
  // current init state. When clients request the current InitState as part of
  // their call to their Init function, the request is put into this TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::FilePath db_dir_;
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;
  std::unique_ptr<LevelDB> db_;

  // Used to return to the Init callback in the case of an error, so we can
  // report corruptions.
  Enums::InitStatus init_status_ = Enums::InitStatus::kNotInitialized;

  std::queue<std::pair<Callbacks::InitStatusCallback,
                       scoped_refptr<base::SequencedTaskRunner>>>
      outstanding_init_requests_;

  std::unique_ptr<base::WeakPtrFactory<SharedProtoDatabase>> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabase);
};

template <typename T>
void GetClientInitCallback(
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
        callback,
    std::unique_ptr<SharedProtoDatabaseClient<T>> client,
    Enums::InitStatus status) {
  // |current_task_runner| is valid because Init already takes the current
  // TaskRunner as a parameter and uses that to trigger this callback when it's
  // finished.
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  if (status != Enums::InitStatus::kOK && status != Enums::InitStatus::kCorrupt)
    client.reset();
  current_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(client)));
}

template <typename T>
void SharedProtoDatabase::GetClientAsync(
    const std::string& client_namespace,
    const std::string& type_prefix,
    bool create_if_missing,
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
        callback) {
  auto client = GetClientInternal<T>(client_namespace, type_prefix);
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, weak_factory_->GetWeakPtr(),
                     create_if_missing,
                     base::BindOnce(&GetClientInitCallback<T>,
                                    std::move(callback), std::move(client)),
                     std::move(current_task_runner)));
}

template <typename T>
std::unique_ptr<SharedProtoDatabaseClient<T>> SharedProtoDatabase::GetClient(
    const std::string& client_namespace,
    const std::string& type_prefix,
    bool create_if_missing,
    Callbacks::InitStatusCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, weak_factory_->GetWeakPtr(),
                     create_if_missing, std::move(callback),
                     std::move(current_task_runner)));
  return GetClientInternal<T>(client_namespace, type_prefix);
}

template <typename T>
std::unique_ptr<SharedProtoDatabaseClient<T>>
SharedProtoDatabase::GetClientInternal(const std::string& client_namespace,
                                       const std::string& type_prefix) {
  return base::WrapUnique(new SharedProtoDatabaseClient<T>(
      std::make_unique<ProtoLevelDBWrapper>(task_runner_, db_.get()),
      client_namespace, type_prefix, this));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_
