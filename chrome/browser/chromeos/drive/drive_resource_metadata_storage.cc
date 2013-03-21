// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

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
    const base::FilePath::StringType& child_name,
    const std::string& child_resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  child_maps_[parent_resource_id][child_name] = child_resource_id;
}

std::string DriveResourceMetadataStorageMemory::GetChild(
    const std::string& parent_resource_id,
    const base::FilePath::StringType& child_name) {
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
    const base::FilePath::StringType& child_name) {
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

}  // namespace drive
