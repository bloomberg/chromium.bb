// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_HTML5_FS_H_
#define LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_HTML5_FS_H_

#include <map>
#include <string>
#include <vector>

#include <ppapi/c/pp_directory_entry.h>

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "nacl_io/pepper_interface_dummy.h"
#include "sdk_util/macros.h"

// This class is a fake implementation of the interfaces necessary to access
// the HTML5 Filesystem from NaCl.
//
// Example:
//   FakePepperInterfaceHtml5Fs ppapi_html5fs;
//   ...
//   PP_Resource ref_resource = ppapi_html5fs.GetFileRefInterface()->Create(
//       ppapi_html5fs.GetInstance(),
//       "/some/path");
//   ...
//
// NOTE: This pepper interface creates an instance resource that can only be
// used with FakePepperInterfaceHtml5Fs, not other fake pepper implementations.

class FakeHtml5FsNode {
 public:
  FakeHtml5FsNode(const PP_FileInfo& info);
  FakeHtml5FsNode(const PP_FileInfo& info,
                  const std::vector<uint8_t>& contents);
  FakeHtml5FsNode(const PP_FileInfo& info, const std::string& contents);

  int32_t Read(int64_t offset, char* buffer, int32_t bytes_to_read);
  int32_t Write(int64_t offset, const char* buffer, int32_t bytes_to_write);
  int32_t Append(const char* buffer, int32_t bytes_to_write);
  int32_t SetLength(int64_t length);
  void GetInfo(PP_FileInfo* out_info);
  bool IsRegular() const;
  bool IsDirectory() const;
  PP_FileType file_type() const { return info_.type; }

  // These times are not modified by the fake implementation.
  void set_creation_time(PP_Time time) { info_.creation_time = time; }
  void set_last_access_time(PP_Time time) { info_.last_access_time = time; }
  void set_last_modified_time(PP_Time time) { info_.last_modified_time = time; }

  const std::vector<uint8_t>& contents() const { return contents_; }

 private:
  PP_FileInfo info_;
  std::vector<uint8_t> contents_;
};

class FakeHtml5FsFilesystem {
 public:
  typedef std::string Path;

  struct DirectoryEntry {
    Path path;
    const FakeHtml5FsNode* node;
  };
  typedef std::vector<DirectoryEntry> DirectoryEntries;

  FakeHtml5FsFilesystem();
  explicit FakeHtml5FsFilesystem(PP_FileSystemType type);
  FakeHtml5FsFilesystem(const FakeHtml5FsFilesystem& filesystem,
                        PP_FileSystemType type);

  void Clear();
  bool AddEmptyFile(const Path& path, FakeHtml5FsNode** out_node);
  bool AddFile(const Path& path,
               const std::string& contents,
               FakeHtml5FsNode** out_node);
  bool AddFile(const Path& path,
               const std::vector<uint8_t>& contents,
               FakeHtml5FsNode** out_node);
  bool AddDirectory(const Path& path, FakeHtml5FsNode** out_node);
  bool RemoveNode(const Path& path);

  FakeHtml5FsNode* GetNode(const Path& path);
  bool GetDirectoryEntries(const Path& path,
                           DirectoryEntries* out_dir_entries) const;
  PP_FileSystemType filesystem_type() const { return filesystem_type_; }
  static Path GetParentPath(const Path& path);

 private:
  typedef std::map<Path, FakeHtml5FsNode> NodeMap;
  NodeMap node_map_;
  PP_FileSystemType filesystem_type_;
};

class FakeFileIoInterface : public nacl_io::FileIoInterface {
 public:
  explicit FakeFileIoInterface(FakeCoreInterface* core_interface);

  virtual PP_Resource Create(PP_Resource instance);
  virtual int32_t Open(PP_Resource file_io,
                       PP_Resource file_ref,
                       int32_t open_flags,
                       PP_CompletionCallback callback);
  virtual int32_t Query(PP_Resource file_io,
                        PP_FileInfo* info,
                        PP_CompletionCallback callback);
  virtual int32_t Read(PP_Resource file_io,
                       int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback);
  virtual int32_t Write(PP_Resource file_io,
                        int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback);
  virtual int32_t SetLength(PP_Resource file_io,
                            int64_t length,
                            PP_CompletionCallback callback);
  virtual int32_t Flush(PP_Resource file_io, PP_CompletionCallback callback);
  virtual void Close(PP_Resource file_io);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(FakeFileIoInterface);
};

class FakeFileRefInterface : public nacl_io::FileRefInterface {
 public:
  FakeFileRefInterface(FakeCoreInterface* core_interface,
                       FakeVarInterface* var_interface);

  virtual PP_Resource Create(PP_Resource file_system, const char* path);
  virtual PP_Var GetName(PP_Resource file_ref);
  virtual int32_t MakeDirectory(PP_Resource directory_ref,
                                PP_Bool make_parents,
                                PP_CompletionCallback callback);
  virtual int32_t Delete(PP_Resource file_ref, PP_CompletionCallback callback);
  virtual int32_t Query(PP_Resource file_ref,
                        PP_FileInfo* info,
                        PP_CompletionCallback callback);
  virtual int32_t ReadDirectoryEntries(PP_Resource file_ref,
                                       const PP_ArrayOutput& output,
                                       PP_CompletionCallback callback);
  virtual int32_t Rename(PP_Resource file_ref,
                         PP_Resource new_file_ref,
                         PP_CompletionCallback callback);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.
  FakeVarInterface* var_interface_;  // Weak reference.
  FakeVarManager* var_manager_;  // Weak reference

  DISALLOW_COPY_AND_ASSIGN(FakeFileRefInterface);
};

class FakeFileSystemInterface : public nacl_io::FileSystemInterface {
 public:
  FakeFileSystemInterface(FakeCoreInterface* core_interface);

  virtual PP_Bool IsFileSystem(PP_Resource resource);
  virtual PP_Resource Create(PP_Instance instance, PP_FileSystemType type);
  virtual int32_t Open(PP_Resource file_system,
                       int64_t expected_size,
                       PP_CompletionCallback callback);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(FakeFileSystemInterface);
};

class FakePepperInterfaceHtml5Fs : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterfaceHtml5Fs();
  explicit FakePepperInterfaceHtml5Fs(const FakeHtml5FsFilesystem& filesystem);
  ~FakePepperInterfaceHtml5Fs();

  virtual PP_Instance GetInstance() { return instance_; }
  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::FileSystemInterface* GetFileSystemInterface();
  virtual nacl_io::FileRefInterface* GetFileRefInterface();
  virtual nacl_io::FileIoInterface* GetFileIoInterface();
  virtual nacl_io::VarInterface* GetVarInterface();

  FakeHtml5FsFilesystem* filesystem_template() { return &filesystem_template_; }

 private:
  void Init();

  FakeResourceManager resource_manager_;
  FakeCoreInterface core_interface_;
  FakeVarInterface var_interface_;
  FakeVarManager var_manager_;
  FakeHtml5FsFilesystem filesystem_template_;
  FakeFileSystemInterface file_system_interface_;
  FakeFileRefInterface file_ref_interface_;
  FakeFileIoInterface file_io_interface_;
  PP_Instance instance_;

  DISALLOW_COPY_AND_ASSIGN(FakePepperInterfaceHtml5Fs);
};

#endif  // LIBRARIES_NACL_IO_TEST_FAKE_PEPPER_INTERFACE_HTML5_FS_H_
