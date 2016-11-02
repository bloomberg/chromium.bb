// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface_html5_fs.h"

#include <string.h>

#include <algorithm>

#include <ppapi/c/pp_errors.h>

#include "gtest/gtest.h"

#include "fake_ppapi/fake_util.h"

FakeHtml5FsNode::FakeHtml5FsNode(const PP_FileInfo& info) : info_(info) {}

FakeHtml5FsNode::FakeHtml5FsNode(const PP_FileInfo& info,
                                 const std::vector<uint8_t>& contents)
    : info_(info), contents_(contents) {}

FakeHtml5FsNode::FakeHtml5FsNode(const PP_FileInfo& info,
                                 const std::string& contents)
    : info_(info) {
  std::copy(contents.begin(), contents.end(), std::back_inserter(contents_));
}

int32_t FakeHtml5FsNode::Read(int64_t offset,
                              char* buffer,
                              int32_t bytes_to_read) {
  if (offset < 0)
    return PP_ERROR_FAILED;

  bytes_to_read =
      std::max(0, std::min<int32_t>(bytes_to_read, contents_.size() - offset));
  memcpy(buffer, contents_.data() + offset, bytes_to_read);
  return bytes_to_read;
}

int32_t FakeHtml5FsNode::Write(int64_t offset,
                               const char* buffer,
                               int32_t bytes_to_write) {
  if (offset < 0)
    return PP_ERROR_FAILED;

  size_t new_size = offset + bytes_to_write;
  if (new_size > contents_.size())
    contents_.resize(new_size);

  memcpy(contents_.data() + offset, buffer, bytes_to_write);
  info_.size = new_size;
  return bytes_to_write;
}

int32_t FakeHtml5FsNode::Append(const char* buffer, int32_t bytes_to_write) {
  return Write(contents_.size(), buffer, bytes_to_write);
}

int32_t FakeHtml5FsNode::SetLength(int64_t length) {
  contents_.resize(length);
  info_.size = length;
  return PP_OK;
}

void FakeHtml5FsNode::GetInfo(PP_FileInfo* out_info) {
  *out_info = info_;
}

bool FakeHtml5FsNode::IsRegular() const {
  return info_.type == PP_FILETYPE_REGULAR;
}

bool FakeHtml5FsNode::IsDirectory() const {
  return info_.type == PP_FILETYPE_DIRECTORY;
}

FakeHtml5FsFilesystem::FakeHtml5FsFilesystem()
    : filesystem_type_(PP_FILESYSTEMTYPE_INVALID) {
  Clear();
}

FakeHtml5FsFilesystem::FakeHtml5FsFilesystem(PP_FileSystemType type)
    : filesystem_type_(type) {
  Clear();
}

FakeHtml5FsFilesystem::FakeHtml5FsFilesystem(
    const FakeHtml5FsFilesystem& filesystem,
    PP_FileSystemType type)
    : node_map_(filesystem.node_map_), filesystem_type_(type) {}

void FakeHtml5FsFilesystem::Clear() {
  node_map_.clear();
  // Always have a root node.
  AddDirectory("/", NULL);
}

bool FakeHtml5FsFilesystem::AddEmptyFile(const Path& path,
                                         FakeHtml5FsNode** out_node) {
  return AddFile(path, std::vector<uint8_t>(), out_node);
}

bool FakeHtml5FsFilesystem::AddFile(const Path& path,
                                    const std::string& contents,
                                    FakeHtml5FsNode** out_node) {
  std::vector<uint8_t> data;
  std::copy(contents.begin(), contents.end(), std::back_inserter(data));
  return AddFile(path, data, out_node);
}

bool FakeHtml5FsFilesystem::AddFile(const Path& path,
                                    const std::vector<uint8_t>& contents,
                                    FakeHtml5FsNode** out_node) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter != node_map_.end()) {
    if (out_node)
      *out_node = NULL;
    return false;
  }

  PP_FileInfo info;
  info.size = contents.size();
  info.type = PP_FILETYPE_REGULAR;
  info.system_type = filesystem_type_;
  info.creation_time = 0;
  info.last_access_time = 0;
  info.last_modified_time = 0;

  FakeHtml5FsNode node(info, contents);
  std::pair<NodeMap::iterator, bool> result =
      node_map_.insert(NodeMap::value_type(path, node));

  EXPECT_EQ(true, result.second);
  if (out_node)
    *out_node = &result.first->second;
  return true;
}

bool FakeHtml5FsFilesystem::AddDirectory(const Path& path,
                                         FakeHtml5FsNode** out_node) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter != node_map_.end()) {
    if (out_node)
      *out_node = NULL;
    return false;
  }

  PP_FileInfo info;
  info.size = 0;
  info.type = PP_FILETYPE_DIRECTORY;
  info.system_type = filesystem_type_;
  info.creation_time = 0;
  info.last_access_time = 0;
  info.last_modified_time = 0;

  FakeHtml5FsNode node(info);
  std::pair<NodeMap::iterator, bool> result =
      node_map_.insert(NodeMap::value_type(path, node));

  EXPECT_EQ(true, result.second);
  if (out_node)
    *out_node = &result.first->second;
  return true;
}

bool FakeHtml5FsFilesystem::RemoveNode(const Path& path) {
  return node_map_.erase(path) >= 1;
}

FakeHtml5FsNode* FakeHtml5FsFilesystem::GetNode(const Path& path) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter == node_map_.end())
    return NULL;
  return &iter->second;
}

bool FakeHtml5FsFilesystem::GetDirectoryEntries(
    const Path& path,
    DirectoryEntries* out_dir_entries) const {
  out_dir_entries->clear();

  NodeMap::const_iterator iter = node_map_.find(path);
  if (iter == node_map_.end())
    return false;

  const FakeHtml5FsNode& dir_node = iter->second;
  if (!dir_node.IsDirectory())
    return false;

  for (NodeMap::const_iterator iter = node_map_.begin();
       iter != node_map_.end(); ++iter) {
    const Path& node_path = iter->first;
    if (node_path.find(path) == std::string::npos)
      continue;

    // A node is not a child of itself.
    if (&iter->second == &dir_node)
      continue;

    // Only consider children, not descendants. If we find a forward slash, then
    // the node must be in a subdirectory.
    if (node_path.find('/', path.size() + 1) != std::string::npos)
      continue;

    // The directory entry names do not include the path.
    Path entry_path = node_path;
    size_t last_slash = node_path.rfind('/');
    if (last_slash != std::string::npos)
      entry_path.erase(0, last_slash + 1);

    DirectoryEntry entry;
    entry.path = entry_path;
    entry.node = &iter->second;
    out_dir_entries->push_back(entry);
  }

  return true;
}

// static
FakeHtml5FsFilesystem::Path FakeHtml5FsFilesystem::GetParentPath(
    const Path& path) {
  size_t last_slash = path.rfind('/');
  if (last_slash == 0)
    return "/";

  EXPECT_EQ(std::string::npos, last_slash);
  return path.substr(0, last_slash);
}

FakePepperInterfaceHtml5Fs::FakePepperInterfaceHtml5Fs()
    : core_interface_(&resource_manager_),
      var_interface_(&var_manager_),
      file_system_interface_(&core_interface_),
      file_ref_interface_(&core_interface_, &var_interface_),
      file_io_interface_(&core_interface_) {
  Init();
}

FakePepperInterfaceHtml5Fs::FakePepperInterfaceHtml5Fs(
    const FakeHtml5FsFilesystem& filesystem)
    : core_interface_(&resource_manager_),
      var_interface_(&var_manager_),
      filesystem_template_(filesystem),
      file_system_interface_(&core_interface_),
      file_ref_interface_(&core_interface_, &var_interface_),
      file_io_interface_(&core_interface_),
      instance_(0) {
  Init();
}

void FakePepperInterfaceHtml5Fs::Init() {
  FakeHtml5FsResource* instance_resource = new FakeHtml5FsResource;
  instance_resource->filesystem_template = &filesystem_template_;

  instance_ = CREATE_RESOURCE(core_interface_.resource_manager(),
                              FakeHtml5FsResource, instance_resource);
}

FakePepperInterfaceHtml5Fs::~FakePepperInterfaceHtml5Fs() {
  core_interface_.ReleaseResource(instance_);
}

nacl_io::CoreInterface* FakePepperInterfaceHtml5Fs::GetCoreInterface() {
  return &core_interface_;
}

nacl_io::FileSystemInterface*
FakePepperInterfaceHtml5Fs::GetFileSystemInterface() {
  return &file_system_interface_;
}

nacl_io::FileRefInterface* FakePepperInterfaceHtml5Fs::GetFileRefInterface() {
  return &file_ref_interface_;
}

nacl_io::FileIoInterface* FakePepperInterfaceHtml5Fs::GetFileIoInterface() {
  return &file_io_interface_;
}

nacl_io::VarInterface* FakePepperInterfaceHtml5Fs::GetVarInterface() {
  return &var_interface_;
}
