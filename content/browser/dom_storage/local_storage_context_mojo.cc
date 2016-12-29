// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/leveldb/public/cpp/util.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/browser/dom_storage/local_storage_database.pb.h"
#include "content/browser/leveldb_wrapper_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "services/file/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

// LevelDB database schema
// =======================
//
// Version 1 (in sorted order):
//   key: "VERSION"
//   value: "1"
//
//   key: "META:" + <url::Origin 'origin'>
//   value: <LocalStorageOriginMetaData serialized as a string>
//
//   key: "_" + <url::Origin> 'origin'> + '\x00' + <script controlled key>
//   value: <script controlled value>

namespace {

const char kVersionKey[] = "VERSION";
const char kOriginSeparator = '\x00';
const char kDataPrefix[] = "_";
const uint8_t kMetaPrefix[] = {'M', 'E', 'T', 'A', ':'};
const int64_t kMinSchemaVersion = 1;
const int64_t kCurrentSchemaVersion = 1;

std::vector<uint8_t> CreateMetaDataKey(const url::Origin& origin) {
  auto serialized_origin = leveldb::StdStringToUint8Vector(origin.Serialize());
  std::vector<uint8_t> key;
  key.reserve(arraysize(kMetaPrefix) + serialized_origin.size());
  key.insert(key.end(), kMetaPrefix, kMetaPrefix + arraysize(kMetaPrefix));
  key.insert(key.end(), serialized_origin.begin(), serialized_origin.end());
  return key;
}

void NoOpSuccess(bool success) {}
}

LocalStorageContextMojo::LocalStorageContextMojo(
    service_manager::Connector* connector,
    const base::FilePath& subdirectory)
    : connector_(connector),
      subdirectory_(subdirectory),
      weak_ptr_factory_(this) {}

LocalStorageContextMojo::~LocalStorageContextMojo() {}

void LocalStorageContextMojo::OpenLocalStorage(
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  RunWhenConnected(base::BindOnce(&LocalStorageContextMojo::BindLocalStorage,
                                  weak_ptr_factory_.GetWeakPtr(), origin,
                                  std::move(request)));
}

void LocalStorageContextMojo::GetStorageUsage(
    GetStorageUsageCallback callback) {
  RunWhenConnected(
      base::BindOnce(&LocalStorageContextMojo::RetrieveStorageUsage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalStorageContextMojo::DeleteStorage(const url::Origin& origin) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&LocalStorageContextMojo::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), origin));
    return;
  }

  LevelDBWrapperImpl* wrapper = GetOrCreateDBWrapper(origin);
  // Renderer process expects |source| to always be two newline separated
  // strings.
  wrapper->DeleteAll("\n", base::Bind(&NoOpSuccess));
  wrapper->ScheduleImmediateCommit();
}

void LocalStorageContextMojo::DeleteStorageForPhysicalOrigin(
    const url::Origin& origin) {
  GetStorageUsage(base::BindOnce(
      &LocalStorageContextMojo::OnGotStorageUsageForDeletePhysicalOrigin,
      weak_ptr_factory_.GetWeakPtr(), origin));
}

void LocalStorageContextMojo::SetDatabaseForTesting(
    leveldb::mojom::LevelDBDatabasePtr database) {
  DCHECK_EQ(connection_state_, NO_CONNECTION);
  connection_state_ = CONNECTION_IN_PROGRESS;
  database_ = std::move(database);
  OnDatabaseOpened(leveldb::mojom::DatabaseError::OK);
}

void LocalStorageContextMojo::RunWhenConnected(base::OnceClosure callback) {
  // If we don't have a filesystem_connection_, we'll need to establish one.
  if (connection_state_ == NO_CONNECTION) {
    CHECK(connector_);
    file_service_connection_ = connector_->Connect(file::mojom::kServiceName);
    connection_state_ = CONNECTION_IN_PROGRESS;
    file_service_connection_->AddConnectionCompletedClosure(
        base::Bind(&LocalStorageContextMojo::OnUserServiceConnectionComplete,
                   weak_ptr_factory_.GetWeakPtr()));
    file_service_connection_->SetConnectionLostClosure(
        base::Bind(&LocalStorageContextMojo::OnUserServiceConnectionError,
                   weak_ptr_factory_.GetWeakPtr()));

    if (!subdirectory_.empty()) {
      // We were given a subdirectory to write to. Get it and use a disk backed
      // database.
      file_service_connection_->GetInterface(&file_system_);
      file_system_->GetSubDirectory(
          subdirectory_.AsUTF8Unsafe(), MakeRequest(&directory_),
          base::Bind(&LocalStorageContextMojo::OnDirectoryOpened,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      // We were not given a subdirectory. Use a memory backed database.
      file_service_connection_->GetInterface(&leveldb_service_);
      leveldb_service_->OpenInMemory(
          MakeRequest(&database_),
          base::Bind(&LocalStorageContextMojo::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (connection_state_ == CONNECTION_IN_PROGRESS) {
    // Queue this OpenLocalStorage call for when we have a level db pointer.
    on_database_opened_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}

void LocalStorageContextMojo::OnLevelDBWrapperHasNoBindings(
    const url::Origin& origin) {
  DCHECK(level_db_wrappers_.find(origin) != level_db_wrappers_.end());
  level_db_wrappers_.erase(origin);
}

std::vector<leveldb::mojom::BatchedOperationPtr>
LocalStorageContextMojo::OnLevelDBWrapperPrepareToCommit(
    const url::Origin& origin,
    const LevelDBWrapperImpl& wrapper) {
  // |wrapper| might not exist in |level_db_wrappers_| anymore at this point, as
  // it is possible this commit was triggered by destruction.

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;

  // Write schema version if not already done so before.
  if (!database_initialized_) {
    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
    item->key = leveldb::StdStringToUint8Vector(kVersionKey);
    item->value = leveldb::StdStringToUint8Vector(
        base::Int64ToString(kCurrentSchemaVersion));
    operations.push_back(std::move(item));
    database_initialized_ = true;
  }

  leveldb::mojom::BatchedOperationPtr item =
      leveldb::mojom::BatchedOperation::New();
  item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
  item->key = CreateMetaDataKey(origin);
  if (wrapper.bytes_used() == 0) {
    item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
  } else {
    item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
    LocalStorageOriginMetaData data;
    data.set_last_modified(base::Time::Now().ToInternalValue());
    data.set_size_bytes(wrapper.bytes_used());
    item->value = leveldb::StdStringToUint8Vector(data.SerializeAsString());
  }
  operations.push_back(std::move(item));

  return operations;
}

void LocalStorageContextMojo::OnUserServiceConnectionComplete() {
  CHECK_EQ(service_manager::mojom::ConnectResult::SUCCEEDED,
           file_service_connection_->GetResult());
}

void LocalStorageContextMojo::OnUserServiceConnectionError() {
  CHECK(false);
}

// Part of our asynchronous directory opening called from OpenLocalStorage().
void LocalStorageContextMojo::OnDirectoryOpened(
    filesystem::mojom::FileError err) {
  if (err != filesystem::mojom::FileError::OK) {
    // We failed to open the directory; continue with startup so that we create
    // the |level_db_wrappers_|.
    OnDatabaseOpened(leveldb::mojom::DatabaseError::IO_ERROR);
    return;
  }

  // Now that we have a directory, connect to the LevelDB service and get our
  // database.
  file_service_connection_->GetInterface(&leveldb_service_);

  auto options = leveldb::mojom::OpenOptions::New();
  options->create_if_missing = true;
  leveldb_service_->OpenWithOptions(
      std::move(options), std::move(directory_), "leveldb",
      MakeRequest(&database_),
      base::Bind(&LocalStorageContextMojo::OnDatabaseOpened,
                 weak_ptr_factory_.GetWeakPtr()));
}

void LocalStorageContextMojo::OnDatabaseOpened(
    leveldb::mojom::DatabaseError status) {
  if (status != leveldb::mojom::DatabaseError::OK) {
    // If we failed to open the database, reset the service object so we pass
    // null pointers to our wrappers.
    database_.reset();
    leveldb_service_.reset();
  }

  // We no longer need the file service; we've either transferred |directory_|
  // to the leveldb service, or we got a file error and no more is possible.
  directory_.reset();
  file_system_.reset();

  // Verify DB schema version.
  if (database_) {
    database_->Get(leveldb::StdStringToUint8Vector(kVersionKey),
                   base::Bind(&LocalStorageContextMojo::OnGotDatabaseVersion,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnGotDatabaseVersion(leveldb::mojom::DatabaseError::IO_ERROR,
                       std::vector<uint8_t>());
}

void LocalStorageContextMojo::OnGotDatabaseVersion(
    leveldb::mojom::DatabaseError status,
    const std::vector<uint8_t>& value) {
  if (status == leveldb::mojom::DatabaseError::NOT_FOUND) {
    // New database, nothing more to do. Current version will get written
    // when first data is committed.
  } else if (status == leveldb::mojom::DatabaseError::OK) {
    // Existing database, check if version number matches current schema
    // version.
    int64_t db_version;
    if (!base::StringToInt64(leveldb::Uint8VectorToStdString(value),
                             &db_version) ||
        db_version < kMinSchemaVersion || db_version > kCurrentSchemaVersion) {
      // TODO(mek): delete and recreate database, rather than failing outright.
      database_ = nullptr;
    }

    database_initialized_ = true;
  } else {
    // Other read error. Possibly database corruption.
    // TODO(mek): delete and recreate database, rather than failing outright.
    database_ = nullptr;
  }

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  for (size_t i = 0; i < on_database_opened_callbacks_.size(); ++i)
    std::move(on_database_opened_callbacks_[i]).Run();
  on_database_opened_callbacks_.clear();
}

// The (possibly delayed) implementation of OpenLocalStorage(). Can be called
// directly from that function, or through |on_database_open_callbacks_|.
void LocalStorageContextMojo::BindLocalStorage(
    const url::Origin& origin,
    mojom::LevelDBWrapperRequest request) {
  GetOrCreateDBWrapper(origin)->Bind(std::move(request));
}

LevelDBWrapperImpl* LocalStorageContextMojo::GetOrCreateDBWrapper(
    const url::Origin& origin) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto found = level_db_wrappers_.find(origin);
  if (found != level_db_wrappers_.end())
    return found->second.get();

  // Delay for a moment after a value is set in anticipation
  // of other values being set, so changes are batched.
  const int kCommitDefaultDelaySecs = 5;

  // To avoid excessive IO we apply limits to the amount of data being written
  // and the frequency of writes.
  const int kMaxBytesPerHour = kPerStorageAreaQuota;
  const int kMaxCommitsPerHour = 60;

  auto wrapper = base::MakeUnique<LevelDBWrapperImpl>(
      database_.get(), kDataPrefix + origin.Serialize() + kOriginSeparator,
      kPerStorageAreaQuota + kPerStorageAreaOverQuotaAllowance,
      base::TimeDelta::FromSeconds(kCommitDefaultDelaySecs), kMaxBytesPerHour,
      kMaxCommitsPerHour,
      base::Bind(&LocalStorageContextMojo::OnLevelDBWrapperHasNoBindings,
                 base::Unretained(this), origin),
      base::Bind(&LocalStorageContextMojo::OnLevelDBWrapperPrepareToCommit,
                 base::Unretained(this), origin));
  LevelDBWrapperImpl* wrapper_ptr = wrapper.get();
  level_db_wrappers_[origin] = std::move(wrapper);
  return wrapper_ptr;
}

void LocalStorageContextMojo::RetrieveStorageUsage(
    GetStorageUsageCallback callback) {
  database_->GetPrefixed(
      std::vector<uint8_t>(kMetaPrefix, kMetaPrefix + arraysize(kMetaPrefix)),
      base::Bind(&LocalStorageContextMojo::OnGotMetaData,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LocalStorageContextMojo::OnGotMetaData(
    GetStorageUsageCallback callback,
    leveldb::mojom::DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> data) {
  std::vector<LocalStorageUsageInfo> result;
  for (const auto& row : data) {
    DCHECK_GT(row->key.size(), arraysize(kMetaPrefix));
    LocalStorageUsageInfo info;
    info.origin = GURL(leveldb::Uint8VectorToStdString(row->key).substr(
        arraysize(kMetaPrefix)));
    if (!info.origin.is_valid()) {
      // TODO(mek): Deal with database corruption.
      continue;
    }

    LocalStorageOriginMetaData data;
    if (!data.ParseFromArray(row->value.data(), row->value.size())) {
      // TODO(mek): Deal with database corruption.
      continue;
    }
    info.data_size = data.size_bytes();
    info.last_modified = base::Time::FromInternalValue(data.last_modified());
    result.push_back(std::move(info));
  }
  std::move(callback).Run(std::move(result));
}

void LocalStorageContextMojo::OnGotStorageUsageForDeletePhysicalOrigin(
    const url::Origin& origin,
    std::vector<LocalStorageUsageInfo> usage) {
  for (const auto& info : usage) {
    url::Origin origin_candidate(info.origin);
    if (!origin_candidate.IsSameOriginWith(origin) &&
        origin_candidate.IsSamePhysicalOriginWith(origin))
      DeleteStorage(origin_candidate);
  }
  DeleteStorage(origin);
}

}  // namespace content
