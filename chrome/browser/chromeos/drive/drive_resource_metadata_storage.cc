// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"

namespace drive {

DriveResourceMetadataStorageMemory::DriveResourceMetadataStorageMemory() {
}

DriveResourceMetadataStorageMemory::~DriveResourceMetadataStorageMemory() {
}

void DriveResourceMetadataStorageMemory::PutEntry(
    const DriveEntryProto& entry) {
  DCHECK(!entry.resource_id().empty());
  resource_map_[entry.resource_id()] = entry;
}

scoped_ptr<DriveEntryProto> DriveResourceMetadataStorageMemory::GetEntry(
    const std::string& resource_id) {
  DCHECK(!resource_id.empty());
  ResourceMap::const_iterator iter = resource_map_.find(resource_id);
  scoped_ptr<DriveEntryProto> entry;
  if (iter != resource_map_.end())
    entry.reset(new DriveEntryProto(iter->second));
  return entry.Pass();
}

void DriveResourceMetadataStorageMemory::RemoveEntry(
    const std::string& resource_id) {
  DCHECK(!resource_id.empty());
  const size_t result = resource_map_.erase(resource_id);
  DCHECK_EQ(1u, result);  // resource_id was found in the map.
}

void DriveResourceMetadataStorageMemory::PutChild(
    const std::string& parent_resource_id,
    const base::FilePath::StringType& child_name,
    const std::string& child_resource_id) {
  child_maps_[parent_resource_id][child_name] = child_resource_id;
}

std::string DriveResourceMetadataStorageMemory::GetChild(
    const std::string& parent_resource_id,
    const base::FilePath::StringType& child_name) {
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
