// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_leveldb.h"

#include <string>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "leveldb/write_batch.h"

namespace gdata {
namespace {

const char kResourceIdPrefix[] = "r:";
const char kPathPrefix[] = "p:";

// Append prefix id: to |resource_id|.
std::string ResourceIdToKey(const std::string& resource_id) {
  return std::string(kResourceIdPrefix) + resource_id;
}

// Append prefix path: to |path|.
std::string PathToKey(const FilePath& path) {
  return std::string(kPathPrefix) + path.value();
}

GDataDB::Status GetStatus(const leveldb::Status& db_status) {
  if (db_status.ok())
    return GDataDB::DB_OK;

  if (db_status.IsNotFound())
    return GDataDB::DB_KEY_NOT_FOUND;

  if (db_status.IsCorruption())
    return GDataDB::DB_CORRUPTION;

  if (db_status.IsIOError())
    return GDataDB::DB_IO_ERROR;

  NOTREACHED();
  return GDataDB::DB_INTERNAL_ERROR;
}

}  // namespace

GDataLevelDB::GDataLevelDB() {
}

GDataLevelDB::~GDataLevelDB() {
}

void GDataLevelDB::Init(const FilePath& db_path) {
  base::ThreadRestrictions::AssertIOAllowed();

  leveldb::DB* level_db = NULL;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status db_status = leveldb::DB::Open(options,
      db_path.Append("level_db").value(), &level_db);
  DCHECK(level_db);
  // TODO(achuith): If db cannot be opened, we should try to recover it.
  // If that fails, we should just delete it and create a new file.
  DCHECK(db_status.ok());
  level_db_.reset(level_db);
}

GDataDB::Status GDataLevelDB::Put(const GDataEntry& entry) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Write the serialized proto.
  std::string serialized_proto;
  entry.SerializeToString(&serialized_proto);

  leveldb::WriteBatch batch;
  const std::string resource_id_key =
      ResourceIdToKey(entry.resource_id());
  const std::string path_key = PathToKey(entry.GetFilePath());
  batch.Put(leveldb::Slice(resource_id_key), leveldb::Slice(serialized_proto));
  // Note we store the resource_id without prefix when it's the value.
  batch.Put(leveldb::Slice(path_key), leveldb::Slice(entry.resource_id()));
  leveldb::Status db_status = level_db_->Write(
      leveldb::WriteOptions(),
      &batch);

  DVLOG(1) << "GDataLevelDB::Put resource_id key = " << resource_id_key
           << ", path key = " << path_key;
  return GetStatus(db_status);
}

GDataDB::Status GDataLevelDB::DeleteByResourceId(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<GDataEntry> entry;
  Status status = GetByResourceId(resource_id, &entry);
  if (status == DB_KEY_NOT_FOUND)
    return DB_OK;
  else if (status != DB_OK)
    return status;

  leveldb::WriteBatch batch;
  const std::string resource_id_key = ResourceIdToKey(resource_id);
  const std::string path_key = PathToKey(entry->GetFilePath());
  batch.Delete(leveldb::Slice(resource_id_key));
  batch.Delete(leveldb::Slice(path_key));

  leveldb::Status db_status = level_db_->Write(leveldb::WriteOptions(),
                                               &batch);
  return GetStatus(db_status);
}

GDataDB::Status GDataLevelDB::DeleteByPath(
    const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string resource_id;
  const Status status = ResourceIdForPath(path, &resource_id);
  if (status != DB_OK)
    return status;
  return DeleteByResourceId(resource_id);
}

GDataDB::Status GDataLevelDB::GetByResourceId(const std::string& resource_id,
                                              scoped_ptr<GDataEntry>* entry) {
  base::ThreadRestrictions::AssertIOAllowed();

  entry->reset();
  std::string serialized_proto;
  const std::string resource_id_key = ResourceIdToKey(resource_id);
  const leveldb::Status db_status = level_db_->Get(leveldb::ReadOptions(),
      leveldb::Slice(resource_id_key), &serialized_proto);

  if (db_status.IsNotFound())
    return DB_KEY_NOT_FOUND;

  if (db_status.ok()) {
    DCHECK(!serialized_proto.empty());
    *entry = GDataEntry::FromProtoString(serialized_proto);
    DCHECK(entry->get());
    return DB_OK;
  }
  return GetStatus(db_status);
}

GDataDB::Status GDataLevelDB::GetByPath(const FilePath& path,
                                        scoped_ptr<GDataEntry>* entry) {
  base::ThreadRestrictions::AssertIOAllowed();

  entry->reset();
  std::string resource_id;
  const Status status = ResourceIdForPath(path, &resource_id);
  if (status != DB_OK)
    return status;
  return GetByResourceId(resource_id, entry);
}

GDataDB::Status GDataLevelDB::ResourceIdForPath(const FilePath& path,
                                                std::string* resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  const std::string path_key = PathToKey(path);
  const leveldb::Status db_status = level_db_->Get(
      leveldb::ReadOptions(), path_key, resource_id);

  return GetStatus(db_status);
}

scoped_ptr<GDataDBIter> GDataLevelDB::CreateIterator(const FilePath& path) {
  return scoped_ptr<GDataDBIter>(new GDataLevelDBIter(
      scoped_ptr<leveldb::Iterator>(
          level_db_->NewIterator(leveldb::ReadOptions())),
      this,
      path));
}

GDataLevelDBIter::GDataLevelDBIter(scoped_ptr<leveldb::Iterator> level_db_iter,
                                   GDataDB* db,
                                   const FilePath& path)
    : level_db_iter_(level_db_iter.Pass()),
      db_(db),
      path_(path) {
  base::ThreadRestrictions::AssertIOAllowed();

  const std::string path_key = PathToKey(path);
  level_db_iter_->Seek(leveldb::Slice(path_key));
}

GDataLevelDBIter::~GDataLevelDBIter() {
}

bool GDataLevelDBIter::GetNext(std::string* path,
                               scoped_ptr<GDataEntry>* entry) {
  base::ThreadRestrictions::AssertIOAllowed();

  DCHECK(path);
  DCHECK(entry);
  path->clear();
  entry->reset();

  if (!level_db_iter_->Valid())
    return false;

  // Only consider keys under |path|.
  const std::string path_key = PathToKey(path_);
  leveldb::Slice key_slice(level_db_iter_->key());
  if (!key_slice.starts_with(path_key))
    return false;

  GDataDB::Status status =
      db_->GetByResourceId(level_db_iter_->value().ToString(), entry);
  DCHECK_EQ(GDataDB::DB_OK, status);

  key_slice.remove_prefix(sizeof(kPathPrefix) - 1);
  path->assign(key_slice.ToString());

  level_db_iter_->Next();
  return true;
}

}  // namespace gdata
