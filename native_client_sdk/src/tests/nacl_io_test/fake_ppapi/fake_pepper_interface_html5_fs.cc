// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface_html5_fs.h"

#include <string.h>

#include <algorithm>

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

#include "gtest/gtest.h"

namespace {

class FakeInstanceResource : public FakeResource {
 public:
  FakeInstanceResource() : filesystem_template(NULL) {}
  static const char* classname() { return "FakeInstanceResource"; }

  FakeHtml5FsFilesystem* filesystem_template;  // Weak reference.
};

class FakeFileSystemResource : public FakeResource {
 public:
  FakeFileSystemResource() : filesystem(NULL), opened(false) {}
  ~FakeFileSystemResource() { delete filesystem; }
  static const char* classname() { return "FakeFileSystemResource"; }

  FakeHtml5FsFilesystem* filesystem;  // Owned.
  bool opened;
};

class FakeFileRefResource : public FakeResource {
 public:
  FakeFileRefResource() : filesystem(NULL) {}
  static const char* classname() { return "FakeFileRefResource"; }

  FakeHtml5FsFilesystem* filesystem;  // Weak reference.
  FakeHtml5FsFilesystem::Path path;
};

class FakeFileIoResource : public FakeResource {
 public:
  FakeFileIoResource() : node(NULL), open_flags(0) {}
  static const char* classname() { return "FakeFileIoResource"; }

  FakeHtml5FsNode* node;  // Weak reference.
  int32_t open_flags;
};

// Helper function to call the completion callback if it is defined (an
// asynchronous call), or return the result directly if it isn't (a synchronous
// call).
//
// Use like this:
//   if (<some error condition>)
//     return RunCompletionCallback(callback, PP_ERROR_FUBAR);
//
//   /* Everything worked OK */
//   return RunCompletionCallback(callback, PP_OK);
int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result) {
  if (callback->func) {
    PP_RunCompletionCallback(callback, result);
    return PP_OK_COMPLETIONPENDING;
  }
  return result;
}

}  // namespace

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

void FakeHtml5FsNode::GetInfo(PP_FileInfo* out_info) { *out_info = info_; }

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
       iter != node_map_.end();
       ++iter) {
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

FakeFileIoInterface::FakeFileIoInterface(FakeCoreInterface* core_interface)
    : core_interface_(core_interface) {}

PP_Resource FakeFileIoInterface::Create(PP_Resource) {
  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileIoResource,
                         new FakeFileIoResource);
}

int32_t FakeFileIoInterface::Open(PP_Resource file_io,
                                  PP_Resource file_ref,
                                  int32_t open_flags,
                                  PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  bool flag_write = !!(open_flags & PP_FILEOPENFLAG_WRITE);
  bool flag_create = !!(open_flags & PP_FILEOPENFLAG_CREATE);
  bool flag_truncate = !!(open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool flag_exclusive = !!(open_flags & PP_FILEOPENFLAG_EXCLUSIVE);
  bool flag_append = !!(open_flags & PP_FILEOPENFLAG_APPEND);

  if ((flag_append && flag_write) || (flag_truncate && !flag_write))
    return PP_ERROR_BADARGUMENT;

  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  const FakeHtml5FsFilesystem::Path& path = file_ref_resource->path;
  FakeHtml5FsFilesystem* filesystem = file_ref_resource->filesystem;
  FakeHtml5FsNode* node = filesystem->GetNode(path);
  bool node_exists = node != NULL;

  if (!node_exists) {
    if (!flag_create)
      return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

    bool result = filesystem->AddEmptyFile(path, &node);
    EXPECT_EQ(true, result);
  } else {
    if (flag_create && flag_exclusive)
      return RunCompletionCallback(&callback, PP_ERROR_FILEEXISTS);
  }

  file_io_resource->node = node;
  file_io_resource->open_flags = open_flags;

  if (flag_truncate)
    return RunCompletionCallback(&callback, node->SetLength(0));

  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileIoInterface::Query(PP_Resource file_io,
                                   PP_FileInfo* info,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  file_io_resource->node->GetInfo(info);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileIoInterface::Read(PP_Resource file_io,
                                  int64_t offset,
                                  char* buffer,
                                  int32_t bytes_to_read,
                                  PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (bytes_to_read < 0)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_READ) !=
      PP_FILEOPENFLAG_READ) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result = file_io_resource->node->Read(offset, buffer, bytes_to_read);
  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::Write(PP_Resource file_io,
                                   int64_t offset,
                                   const char* buffer,
                                   int32_t bytes_to_write,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_WRITE) !=
      PP_FILEOPENFLAG_WRITE) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result;
  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_APPEND) ==
      PP_FILEOPENFLAG_APPEND) {
    result = file_io_resource->node->Append(buffer, bytes_to_write);
  } else {
    result = file_io_resource->node->Write(offset, buffer, bytes_to_write);
  }

  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::SetLength(PP_Resource file_io,
                                       int64_t length,
                                       PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if ((file_io_resource->open_flags & PP_FILEOPENFLAG_WRITE) !=
      PP_FILEOPENFLAG_WRITE) {
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);
  }

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  int32_t result = file_io_resource->node->SetLength(length);
  return RunCompletionCallback(&callback, result);
}

int32_t FakeFileIoInterface::Flush(PP_Resource file_io,
                                   PP_CompletionCallback callback) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_io_resource->node)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  return RunCompletionCallback(&callback, PP_OK);
}

void FakeFileIoInterface::Close(PP_Resource file_io) {
  FakeFileIoResource* file_io_resource =
      core_interface_->resource_manager()->Get<FakeFileIoResource>(file_io);
  if (file_io_resource == NULL)
    return;

  file_io_resource->node = NULL;
  file_io_resource->open_flags = 0;
}

FakeFileRefInterface::FakeFileRefInterface(FakeCoreInterface* core_interface,
                                           FakeVarInterface* var_interface)
    : core_interface_(core_interface), var_interface_(var_interface) {}

PP_Resource FakeFileRefInterface::Create(PP_Resource file_system,
                                         const char* path) {
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          file_system);
  if (file_system_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!file_system_resource->opened)
    return PP_ERROR_FAILED;

  if (path == NULL)
    return PP_ERROR_FAILED;

  size_t path_len = strlen(path);
  if (path_len == 0)
    return PP_ERROR_FAILED;

  FakeFileRefResource* file_ref_resource = new FakeFileRefResource;
  file_ref_resource->filesystem = file_system_resource->filesystem;
  file_ref_resource->path = path;

  // Remove a trailing slash from the path, unless it is the root path.
  if (path_len > 1 && file_ref_resource->path[path_len - 1] == '/')
    file_ref_resource->path.erase(path_len - 1);

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileRefResource,
                         file_ref_resource);
}

PP_Var FakeFileRefInterface::GetName(PP_Resource file_ref) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_MakeUndefined();

  return var_interface_->VarFromUtf8(file_ref_resource->path.c_str(),
                                     file_ref_resource->path.size());
}

int32_t FakeFileRefInterface::MakeDirectory(PP_Resource directory_ref,
                                            PP_Bool make_parents,
                                            PP_CompletionCallback callback) {
  FakeFileRefResource* directory_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          directory_ref);
  if (directory_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // TODO(binji): We don't currently use make_parents in nacl_io, so
  // I won't bother handling it yet.
  if (make_parents == PP_TRUE)
    return PP_ERROR_FAILED;

  FakeHtml5FsFilesystem* filesystem = directory_ref_resource->filesystem;
  FakeHtml5FsFilesystem::Path path = directory_ref_resource->path;

  // Pepper returns PP_ERROR_NOACCESS when trying to create the root directory,
  // not PP_ERROR_FILEEXISTS, as you might expect.
  if (path == "/")
    return RunCompletionCallback(&callback, PP_ERROR_NOACCESS);

  FakeHtml5FsNode* node = filesystem->GetNode(path);
  if (node != NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILEEXISTS);

  FakeHtml5FsFilesystem::Path parent_path = filesystem->GetParentPath(path);
  FakeHtml5FsNode* parent_node = filesystem->GetNode(parent_path);
  if (parent_node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  if (!parent_node->IsDirectory())
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  bool result = filesystem->AddDirectory(directory_ref_resource->path, NULL);
  EXPECT_EQ(true, result);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Delete(PP_Resource file_ref,
                                     PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeHtml5FsFilesystem* filesystem = file_ref_resource->filesystem;
  FakeHtml5FsFilesystem::Path path = file_ref_resource->path;
  FakeHtml5FsNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  filesystem->RemoveNode(path);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Query(PP_Resource file_ref,
                                    PP_FileInfo* info,
                                    PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeHtml5FsFilesystem* filesystem = file_ref_resource->filesystem;
  FakeHtml5FsFilesystem::Path path = file_ref_resource->path;
  FakeHtml5FsNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  node->GetInfo(info);
  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::ReadDirectoryEntries(
    PP_Resource directory_ref,
    const PP_ArrayOutput& output,
    PP_CompletionCallback callback) {
  FakeFileRefResource* directory_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          directory_ref);
  if (directory_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeHtml5FsFilesystem* filesystem = directory_ref_resource->filesystem;
  FakeHtml5FsFilesystem::Path path = directory_ref_resource->path;
  FakeHtml5FsNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);

  if (!node->IsDirectory())
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  FakeHtml5FsFilesystem::DirectoryEntries fake_dir_entries;
  filesystem->GetDirectoryEntries(path, &fake_dir_entries);

  uint32_t element_count = fake_dir_entries.size();
  uint32_t element_size = sizeof(fake_dir_entries[0]);
  void* data_buffer =
      (*output.GetDataBuffer)(output.user_data, element_count, element_size);

  if (data_buffer == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FAILED);

  PP_DirectoryEntry* dir_entries = static_cast<PP_DirectoryEntry*>(data_buffer);
  for (uint32_t i = 0; i < element_count; ++i) {
    const FakeHtml5FsFilesystem::DirectoryEntry& fake_dir_entry =
        fake_dir_entries[i];

    FakeFileRefResource* file_ref_resource = new FakeFileRefResource;
    file_ref_resource->filesystem = directory_ref_resource->filesystem;
    file_ref_resource->path = fake_dir_entry.path;
    PP_Resource file_ref = CREATE_RESOURCE(core_interface_->resource_manager(),
                                           FakeFileRefResource,
                                           file_ref_resource);

    dir_entries[i].file_ref = file_ref;
    dir_entries[i].file_type = fake_dir_entry.node->file_type();
  }

  return RunCompletionCallback(&callback, PP_OK);
}

int32_t FakeFileRefInterface::Rename(PP_Resource file_ref,
                                     PP_Resource new_file_ref,
                                     PP_CompletionCallback callback) {
  FakeFileRefResource* file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(file_ref);
  if (file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFileRefResource* new_file_ref_resource =
      core_interface_->resource_manager()->Get<FakeFileRefResource>(
          new_file_ref);
  if (new_file_ref_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeHtml5FsFilesystem* filesystem = file_ref_resource->filesystem;
  FakeHtml5FsFilesystem::Path path = file_ref_resource->path;
  FakeHtml5FsFilesystem::Path newpath = new_file_ref_resource->path;
  FakeHtml5FsNode* node = filesystem->GetNode(path);
  if (node == NULL)
    return RunCompletionCallback(&callback, PP_ERROR_FILENOTFOUND);
  // FakeFileRefResource does not support directory rename.
  if (!node->IsRegular())
    return RunCompletionCallback(&callback, PP_ERROR_NOTAFILE);

  // Remove the destination if it exists.
  filesystem->RemoveNode(newpath);
  const std::vector<uint8_t> contents = node->contents();
  EXPECT_TRUE(filesystem->AddFile(newpath, contents, NULL));
  EXPECT_TRUE(filesystem->RemoveNode(path));
  return RunCompletionCallback(&callback, PP_OK);
}

FakeFileSystemInterface::FakeFileSystemInterface(
    FakeCoreInterface* core_interface)
    : core_interface_(core_interface) {}

PP_Bool FakeFileSystemInterface::IsFileSystem(PP_Resource resource) {
  bool not_found_ok = true;
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          resource, not_found_ok);
  return file_system_resource != NULL ? PP_TRUE : PP_FALSE;
}

PP_Resource FakeFileSystemInterface::Create(PP_Instance instance,
                                            PP_FileSystemType filesystem_type) {
  FakeInstanceResource* instance_resource =
      core_interface_->resource_manager()->Get<FakeInstanceResource>(instance);
  if (instance_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeFileSystemResource* file_system_resource = new FakeFileSystemResource;
  file_system_resource->filesystem = new FakeHtml5FsFilesystem(
      *instance_resource->filesystem_template, filesystem_type);

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeFileSystemResource,
                         file_system_resource);
}

int32_t FakeFileSystemInterface::Open(PP_Resource file_system,
                                      int64_t expected_size,
                                      PP_CompletionCallback callback) {
  FakeFileSystemResource* file_system_resource =
      core_interface_->resource_manager()->Get<FakeFileSystemResource>(
          file_system);
  if (file_system_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  file_system_resource->opened = true;
  return RunCompletionCallback(&callback, PP_OK);
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
  FakeInstanceResource* instance_resource = new FakeInstanceResource;
  instance_resource->filesystem_template = &filesystem_template_;

  instance_ = CREATE_RESOURCE(core_interface_.resource_manager(),
                              FakeInstanceResource,
                              instance_resource);
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
