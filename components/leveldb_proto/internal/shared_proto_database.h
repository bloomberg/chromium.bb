// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_

#include <queue>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto/shared_db_metadata.pb.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/internal/shared_proto_database_client.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

class SharedDBMetadataProto;

// Controls a single LevelDB database to be used by many clients, and provides
// a way to get SharedProtoDatabaseClients that allow shared access to the
// underlying single database.
class SharedProtoDatabase
    : public base::RefCountedThreadSafe<SharedProtoDatabase> {
 public:
  using SharedClientInitCallback =
      base::OnceCallback<void(Enums::InitStatus,
                              SharedDBMetadataProto::MigrationStatus)>;

  // Always returns a SharedProtoDatabaseClient pointer, but that should ONLY
  // be used if the callback returns success.
  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClientForTesting(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      SharedClientInitCallback callback);

  // A version of GetClient that returns the client in a callback instead of
  // giving back a client instance immediately.
  template <typename T>
  void GetClientAsync(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
          callback);

  void GetDatabaseInitStatusAsync(const std::string& client_db_id,
                                  Callbacks::InitStatusCallback callback);

  void UpdateClientMetadataAsync(
      const std::string& client_db_id,
      SharedDBMetadataProto::MigrationStatus migration_status,
      Callbacks::UpdateCallback callback);

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

  struct InitRequest {
    InitRequest(SharedClientInitCallback callback,
                const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                const std::string& client_db_id);

    ~InitRequest();

    SharedClientInitCallback callback;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    std::string client_db_id;
  };

  // Make sure to give enough time after startup so that we have less chance of
  // affecting startup or navigations.
  static const base::TimeDelta kDelayToClearObsoleteDatabase;

  // Private since we only want to create a singleton of it.
  SharedProtoDatabase(const std::string& client_db_id,
                      const base::FilePath& db_dir);

  virtual ~SharedProtoDatabase();

  void ProcessInitRequests(Enums::InitStatus status);

  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClientInternal(
      const std::string& client_namespace,
      const std::string& type_prefix);

  void OnGetClientMetadata(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      bool success,
      std::unique_ptr<SharedDBMetadataProto> proto);

  // |callback_task_runner| should be the same sequence that Init was called
  // from.
  void Init(bool create_if_missing,
            const std::string& client_db_id,
            SharedClientInitCallback callback,
            scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void InitMetadataDatabase(bool create_shared_db_if_missing,
                            int attempt,
                            bool corruption);
  void OnMetadataInitComplete(bool create_shared_db_if_missing,
                              int attempt,
                              bool corruption,
                              Enums::InitStatus status);
  void OnGetGlobalMetadata(bool create_shared_db_if_missing,
                           bool corruption,
                           bool success,
                           std::unique_ptr<SharedDBMetadataProto> proto);
  void OnFinishCorruptionCountWrite(bool create_shared_db_if_missing,
                                    bool success);
  void InitDatabase(bool create_shared_db_if_missing);
  void OnDatabaseInit(Enums::InitStatus status);
  void CheckCorruptionAndRunInitCallback(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      Enums::InitStatus status);
  void GetClientMetadataAsync(
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void OnUpdateCorruptionCountAtInit(bool success);

  void CommitUpdatedGlobalMetadata(Callbacks::UpdateCallback callback);

  void RunInitCallback(
      Callbacks::InitStatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  LevelDB* GetLevelDBForTesting() const;

  scoped_refptr<base::SequencedTaskRunner> database_task_runner_for_testing()
      const {
    return task_runner_;
  }

  SEQUENCE_CHECKER(on_task_runner_);

  InitState init_state_ = InitState::kNone;

  // This TaskRunner is used to properly sequence Init calls and checks for the
  // current init state. When clients request the current InitState as part of
  // their call to their Init function, the request is put into this TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath db_dir_;
  std::unique_ptr<LevelDB> db_;
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;

  std::unique_ptr<LevelDB> metadata_db_;
  std::unique_ptr<ProtoLevelDBWrapper> metadata_db_wrapper_;
  std::unique_ptr<SharedDBMetadataProto> metadata_;

  // Used to return to the Init callback in the case of an error, so we can
  // report corruptions.
  Enums::InitStatus init_status_ = Enums::InitStatus::kNotInitialized;

  std::queue<std::unique_ptr<InitRequest>> outstanding_init_requests_;

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabase);
};

template <typename T>
void GetClientInitCallback(
    base::OnceCallback<void(std::unique_ptr<SharedProtoDatabaseClient<T>>)>
        callback,
    std::unique_ptr<SharedProtoDatabaseClient<T>> client,
    Enums::InitStatus status,
    SharedDBMetadataProto::MigrationStatus migration_status) {
  // |current_task_runner| is valid because Init already takes the current
  // TaskRunner as a parameter and uses that to trigger this callback when it's
  // finished.
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  if (status != Enums::InitStatus::kOK && status != Enums::InitStatus::kCorrupt)
    client.reset();
  // Set migration status of client. The metadata database was already updated.
  if (client)
    client->set_migration_status(migration_status);
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
  SharedProtoDatabaseClient<T>* client_ptr = client.get();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, this, create_if_missing,
                     client_ptr->client_db_id(),
                     base::BindOnce(&GetClientInitCallback<T>,
                                    std::move(callback), std::move(client)),
                     std::move(current_task_runner)));
}

// TODO(thildebr): Need to pass the client name into this call as well, and use
// it with the pending requests too so we can clean up the database.
template <typename T>
std::unique_ptr<SharedProtoDatabaseClient<T>>
SharedProtoDatabase::GetClientForTesting(const std::string& client_namespace,
                                         const std::string& type_prefix,
                                         bool create_if_missing,
                                         SharedClientInitCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  auto client = GetClientInternal<T>(client_namespace, type_prefix);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, this, create_if_missing,
                     client->client_db_id(), std::move(callback),
                     std::move(current_task_runner)));
  return client;
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

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_H_
