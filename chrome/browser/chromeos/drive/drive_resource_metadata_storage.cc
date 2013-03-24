// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"

#include <leveldb/db.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

namespace {

const base::FilePath::CharType kResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_resource_map.db");
const base::FilePath::CharType kChildMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_child_map.db");

// Meant to be a character which never happen to be in real resource IDs.
const char kDBKeyDelimeter = '\0';

// Returns a string to be used as the key for the header.
std::string GetHeaderDBKey() {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append("HEADER");
  return key;
}

// Returns a string to be used as keys for child map.
std::string GetChildMapKey(const std::string& parent_resource_id,
                           const std::string& child_name) {
  std::string key = parent_resource_id;
  key.push_back(kDBKeyDelimeter);
  key.append(child_name);
  return key;
}

}  // namespace

DriveResourceMetadataStorageMemory::DriveResourceMetadataStorageMemory()
    : largest_changestamp_(0) {
}

DriveResourceMetadataStorageMemory::~DriveResourceMetadataStorageMemory() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool DriveResourceMetadataStorageMemory::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();
  // Do nothing.
  return true;
}

bool DriveResourceMetadataStorageMemory::IsPersistentStorage() {
  base::ThreadRestrictions::AssertIOAllowed();
  return false;
}

void DriveResourceMetadataStorageMemory::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();
  largest_changestamp_ = largest_changestamp;
}

int64 DriveResourceMetadataStorageMemory::GetLargestChangestamp() {
  base::ThreadRestrictions::AssertIOAllowed();
  return largest_changestamp_;
}

void DriveResourceMetadataStorageMemory::PutEntry(
    const DriveEntryProto& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!entry.resource_id().empty());
  resource_map_[entry.resource_id()] = entry;
}

scoped_ptr<DriveEntryProto> DriveResourceMetadataStorageMemory::GetEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  ResourceMap::const_iterator iter = resource_map_.find(resource_id);
  scoped_ptr<DriveEntryProto> entry;
  if (iter != resource_map_.end())
    entry.reset(new DriveEntryProto(iter->second));
  return entry.Pass();
}

void DriveResourceMetadataStorageMemory::RemoveEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  const size_t result = resource_map_.erase(resource_id);
  DCHECK_EQ(1u, result);  // resource_id was found in the map.
}

void DriveResourceMetadataStorageMemory::PutChild(
    const std::string& parent_resource_id,
    const std::string& child_name,
    const std::string& child_resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  child_maps_[parent_resource_id][child_name] = child_resource_id;
}

std::string DriveResourceMetadataStorageMemory::GetChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::const_iterator iter = child_maps_.find(parent_resource_id);
  if (iter == child_maps_.end())
    return std::string();
  const ChildMap& child_map = iter->second;
  ChildMap::const_iterator sub_iter = child_map.find(child_name);
  if (sub_iter == child_map.end())
    return std::string();
  return sub_iter->second;
}

void DriveResourceMetadataStorageMemory::GetChildren(
    const std::string& parent_resource_id,
    std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::const_iterator iter = child_maps_.find(parent_resource_id);
  if (iter == child_maps_.end())
    return;

  const ChildMap& child_map = iter->second;
  for (ChildMap::const_iterator sub_iter = child_map.begin();
       sub_iter != child_map.end(); ++sub_iter)
    children->push_back(sub_iter->second);
}

void DriveResourceMetadataStorageMemory::RemoveChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::iterator iter = child_maps_.find(parent_resource_id);
  DCHECK(iter != child_maps_.end());

  ChildMap* child_map = &iter->second;
  const size_t result = child_map->erase(child_name);
  DCHECK_EQ(1u, result);  // child_name was found in the map.

  // Erase the map if it got empty.
  if (child_map->empty())
    child_maps_.erase(iter);
}

DriveResourceMetadataStorageDB::DriveResourceMetadataStorageDB(
    const base::FilePath& directory_path)
    : directory_path_(directory_path) {
}

DriveResourceMetadataStorageDB::~DriveResourceMetadataStorageDB() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool DriveResourceMetadataStorageDB::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();

  const base::FilePath resource_map_path =
      directory_path_.Append(kResourceMapDBName);
  const base::FilePath child_map_path = directory_path_.Append(kChildMapDBName);

  // Try to open the existing DB.
  leveldb::DB* db = NULL;
  leveldb::Options options;
  options.create_if_missing = false;

  leveldb::Status status =
      leveldb::DB::Open(options, resource_map_path.value(), &db);
  if (status.ok())
    resource_map_.reset(db);

  status = leveldb::DB::Open(options, child_map_path.value(), &db);
  if (status.ok())
    child_map_.reset(db);

  // Check the version of existing DB.
  if (resource_map_ && child_map_) {
    scoped_ptr<DriveResourceMetadataHeader> header = GetHeader();
    if (!header ||
        header->version() != kDBVersion) {
      LOG(ERROR) << "Reject incompatible DB.";
      resource_map_.reset();
      child_map_.reset();
    }
  }

  // Failed to open the existing DB, create new DB.
  if (!resource_map_ || !child_map_) {
    resource_map_.reset();
    child_map_.reset();

    // Clean up the destination.
    const bool kRecursive = true;
    file_util::Delete(resource_map_path, kRecursive);
    file_util::Delete(child_map_path, kRecursive);

    // Create DBs.
    options.create_if_missing = true;

    status = leveldb::DB::Open(options, resource_map_path.value(), &db);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to create resource map DB: " << status.ToString();
      return false;
    }
    resource_map_.reset(db);

    status = leveldb::DB::Open(options, child_map_path.value(), &db);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to create child map DB: " << status.ToString();
      resource_map_.reset();
      return false;
    }
    child_map_.reset(db);

    // Set up header.
    DriveResourceMetadataHeader header;
    header.set_version(kDBVersion);
    PutHeader(header);
  }

  DCHECK(resource_map_);
  DCHECK(child_map_);
  return true;
}

bool DriveResourceMetadataStorageDB::IsPersistentStorage() {
  base::ThreadRestrictions::AssertIOAllowed();
  return true;
}

void DriveResourceMetadataStorageDB::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<DriveResourceMetadataHeader> header = GetHeader();
  DCHECK(header);
  header->set_largest_changestamp(largest_changestamp);
  PutHeader(*header);
}

int64 DriveResourceMetadataStorageDB::GetLargestChangestamp() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_ptr<DriveResourceMetadataHeader> header = GetHeader();
  DCHECK(header);
  return header->largest_changestamp();
}

void DriveResourceMetadataStorageDB::PutEntry(
    const DriveEntryProto& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!entry.resource_id().empty());

  std::string serialized_entry;
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry: " << entry.resource_id();
    return;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(entry.resource_id()),
      leveldb::Slice(serialized_entry));
  DCHECK(status.ok());
}

scoped_ptr<DriveEntryProto> DriveResourceMetadataStorageDB::GetEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                                    leveldb::Slice(resource_id),
                                                    &serialized_entry);
  if (!status.ok())
    return scoped_ptr<DriveEntryProto>();

  scoped_ptr<DriveEntryProto> entry(new DriveEntryProto);
  if (!entry->ParseFromString(serialized_entry))
    return scoped_ptr<DriveEntryProto>();
  return entry.Pass();
}

void DriveResourceMetadataStorageDB::RemoveEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  const leveldb::Status status = resource_map_->Delete(
      leveldb::WriteOptions(),
      leveldb::Slice(resource_id));
  DCHECK(status.ok());
}

void DriveResourceMetadataStorageDB::PutChild(
    const std::string& parent_resource_id,
    const std::string& child_name,
    const std::string& child_resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  const leveldb::Status status = child_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetChildMapKey(parent_resource_id, child_name)),
      leveldb::Slice(child_resource_id));
  DCHECK(status.ok());
}

std::string DriveResourceMetadataStorageDB::GetChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string child_resource_id;
  child_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetChildMapKey(parent_resource_id, child_name)),
      &child_resource_id);
  return child_resource_id;
}

void DriveResourceMetadataStorageDB::GetChildren(
    const std::string& parent_resource_id,
    std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Iterate over all entries with keys starting with |parent_resource_id|.
  scoped_ptr<leveldb::Iterator> it(
      child_map_->NewIterator(leveldb::ReadOptions()));
  for (it->Seek(parent_resource_id); it->Valid(); it->Next()) {
    if (!StartsWithASCII(it->key().ToString(),
                         parent_resource_id,
                         true /* case_sensitive */))
      break;

    children->push_back(it->value().ToString());
  }
  DCHECK(it->status().ok());
}

void DriveResourceMetadataStorageDB::RemoveChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  const leveldb::Status status = child_map_->Delete(
      leveldb::WriteOptions(),
      leveldb::Slice(GetChildMapKey(parent_resource_id, child_name)));
  DCHECK(status.ok());
}

void DriveResourceMetadataStorageDB::PutHeader(
    const DriveResourceMetadataHeader& header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  if (!header.SerializeToString(&serialized_header)) {
    DLOG(ERROR) << "Failed to serialize the header";
    return;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      leveldb::Slice(serialized_header));
  DCHECK(status.ok());
}

scoped_ptr<DriveResourceMetadataHeader>
DriveResourceMetadataStorageDB::GetHeader() {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      &serialized_header);
  if (!status.ok())
    return scoped_ptr<DriveResourceMetadataHeader>();

  scoped_ptr<DriveResourceMetadataHeader> header(
      new DriveResourceMetadataHeader);
  if (!header->ParseFromString(serialized_header))
    return scoped_ptr<DriveResourceMetadataHeader>();
  return header.Pass();
}

}  // namespace drive
