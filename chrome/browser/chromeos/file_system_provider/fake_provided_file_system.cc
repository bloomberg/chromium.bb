// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"

#include <string>

#include "base/files/file.h"
#include "base/message_loop/message_loop_proxy.h"
#include "extensions/browser/event_router.h"
#include "net/base/io_buffer.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Adds a fake entry to the entry list.
void AddDirectoryEntry(fileapi::AsyncFileUtil::EntryList* entry_list,
                       const std::string& name,
                       fileapi::DirectoryEntry::DirectoryEntryType type,
                       int64 size,
                       std::string last_modified_time_string) {
  base::Time last_modified_time;
  const bool result = base::Time::FromString(last_modified_time_string.c_str(),
                                             &last_modified_time);
  DCHECK(result);
  entry_list->push_back(
      fileapi::DirectoryEntry(name, type, size, last_modified_time));
}

}  // namespace

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info), last_file_handle_(0) {
}

FakeProvidedFileSystem::~FakeProvidedFileSystem() {}

void FakeProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK));
}

void FakeProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) {
  // Return fake metadata for the root directory only.
  if (entry_path.AsUTF8Unsafe() != "/") {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, base::File::FILE_ERROR_NOT_FOUND, base::File::Info()));
    return;
  }

  base::File::Info file_info;
  file_info.size = 0;
  file_info.is_directory = true;
  file_info.is_symbolic_link = false;
  base::Time last_modified_time;
  const bool result = base::Time::FromString("Thu Apr 24 00:46:52 UTC 2014",
                                             &last_modified_time);
  DCHECK(result);
  file_info.last_modified = last_modified_time;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK, file_info));
}

void FakeProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) {
  // Return fake contents for the root directory only.
  if (directory_path.AsUTF8Unsafe() != "/") {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   base::File::FILE_ERROR_NOT_FOUND,
                   fileapi::AsyncFileUtil::EntryList(),
                   false /* has_more */));
    return;
  }

  {
    fileapi::AsyncFileUtil::EntryList entry_list;
    AddDirectoryEntry(&entry_list,
                      "hello.txt",
                      fileapi::DirectoryEntry::FILE,
                      1024 /* size */,
                      "Thu Apr 24 00:46:52 UTC 2014");

    AddDirectoryEntry(&entry_list,
                      "world.txt",
                      fileapi::DirectoryEntry::FILE,
                      1024 /* size */,
                      "Wed Apr 23 00:20:30 UTC 2014");

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, base::File::FILE_OK, entry_list, true /* has_more */));
  }

  {
    fileapi::AsyncFileUtil::EntryList entry_list;
    AddDirectoryEntry(&entry_list,
                      "pictures",
                      fileapi::DirectoryEntry::DIRECTORY,
                      0 /* size */,
                      "Tue May 22 00:40:50 UTC 2014");

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, base::File::FILE_OK, entry_list, false /* has_more */));
  }
}

void FakeProvidedFileSystem::OpenFile(const base::FilePath& file_path,
                                      OpenFileMode mode,
                                      bool create,
                                      const OpenFileCallback& callback) {
  if (file_path.AsUTF8Unsafe() != "/hello.txt" ||
      mode == OPEN_FILE_MODE_WRITE || create) {
    callback.Run(0 /* file_handle */, base::File::FILE_ERROR_SECURITY);
    return;
  }

  const int file_handle = ++last_file_handle_;
  opened_files_.insert(file_handle);
  callback.Run(file_handle, base::File::FILE_OK);
}

void FakeProvidedFileSystem::CloseFile(
    int file_handle,
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  const std::set<int>::iterator opened_file_it =
      opened_files_.find(file_handle);
  if (opened_file_it == opened_files_.end()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  opened_files_.erase(opened_file_it);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK));
}

void FakeProvidedFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const ProvidedFileSystemInterface::ReadChunkReceivedCallback& callback) {
  // TODO(mtomasz): Implement together with the FileStreamReader.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 0 /* chunk_length */,
                 false /* has_next */,
                 base::File::FILE_ERROR_SECURITY));
}

const ProvidedFileSystemInfo& FakeProvidedFileSystem::GetFileSystemInfo()
    const {
  return file_system_info_;
}

RequestManager* FakeProvidedFileSystem::GetRequestManager() {
  NOTREACHED();
  return NULL;
}

ProvidedFileSystemInterface* FakeProvidedFileSystem::Create(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info) {
  return new FakeProvidedFileSystem(file_system_info);
}

}  // namespace file_system_provider
}  // namespace chromeos
