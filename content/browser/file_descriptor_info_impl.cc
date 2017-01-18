// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_descriptor_info_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace content {

// static
std::unique_ptr<FileDescriptorInfo> FileDescriptorInfoImpl::Create() {
  return std::unique_ptr<FileDescriptorInfo>(new FileDescriptorInfoImpl());
}

FileDescriptorInfoImpl::FileDescriptorInfoImpl() {
}

FileDescriptorInfoImpl::~FileDescriptorInfoImpl() {
}

void FileDescriptorInfoImpl::Share(int id, base::PlatformFile fd) {
  ShareWithRegion(id, fd, base::MemoryMappedFile::Region::kWholeFile);
}

void FileDescriptorInfoImpl::ShareWithRegion(int id, base::PlatformFile fd,
    const base::MemoryMappedFile::Region& region) {
  AddToMapping(id, fd, region);
}

void FileDescriptorInfoImpl::Transfer(int id, base::ScopedFD fd) {
  AddToMapping(id, fd.get(), base::MemoryMappedFile::Region::kWholeFile);
  owned_descriptors_.push_back(std::move(fd));
}

base::PlatformFile FileDescriptorInfoImpl::GetFDAt(size_t i) const {
  return mapping_[i].first;
}

int FileDescriptorInfoImpl::GetIDAt(size_t i) const {
  return mapping_[i].second;
}

const base::MemoryMappedFile::Region& FileDescriptorInfoImpl::GetRegionAt(
    size_t i) const {
  auto iter = ids_to_regions_.find(GetIDAt(i));
  return (iter != ids_to_regions_.end()) ?
      iter->second : base::MemoryMappedFile::Region::kWholeFile;
}

size_t FileDescriptorInfoImpl::GetMappingSize() const {
  return mapping_.size();
}

bool FileDescriptorInfoImpl::HasID(int id) const {
  for (unsigned i = 0; i < mapping_.size(); ++i) {
    if (mapping_[i].second == id)
      return true;
  }

  return false;
}

bool FileDescriptorInfoImpl::OwnsFD(base::PlatformFile file) const {
  return base::ContainsValue(owned_descriptors_, file);
}

base::ScopedFD FileDescriptorInfoImpl::ReleaseFD(base::PlatformFile file) {
  DCHECK(OwnsFD(file));

  base::ScopedFD fd;
  auto found =
      std::find(owned_descriptors_.begin(), owned_descriptors_.end(), file);

  std::swap(*found, fd);
  owned_descriptors_.erase(found);

  return fd;
}

void FileDescriptorInfoImpl::AddToMapping(int id, base::PlatformFile fd,
    const base::MemoryMappedFile::Region& region) {
  DCHECK(!HasID(id));
  mapping_.push_back(std::make_pair(fd, id));
  if (region != base::MemoryMappedFile::Region::kWholeFile)
    ids_to_regions_[id] = region;
}

const base::FileHandleMappingVector& FileDescriptorInfoImpl::GetMapping()
    const {
  return mapping_;
}

std::unique_ptr<base::FileHandleMappingVector>
FileDescriptorInfoImpl::GetMappingWithIDAdjustment(int delta) const {
  std::unique_ptr<base::FileHandleMappingVector> result =
      base::MakeUnique<base::FileHandleMappingVector>(mapping_);
  // Adding delta to each ID.
  for (unsigned i = 0; i < mapping_.size(); ++i)
    (*result)[i].second += delta;
  return result;
}

}  // namespace content
