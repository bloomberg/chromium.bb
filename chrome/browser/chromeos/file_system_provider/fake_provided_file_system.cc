// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"

#include <string>

#include "base/files/file.h"
#include "base/message_loop/message_loop_proxy.h"
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

const char kFakeFileName[] = "hello.txt";
const char kFakeFilePath[] = "/hello.txt";
const char kFakeFileText[] =
    "This is a testing file. Lorem ipsum dolor sit amet est.";
const size_t kFakeFileSize = sizeof(kFakeFileText) - 1u;
const char kFakeFileModificationTime[] = "Fri Apr 25 01:47:53 UTC 2014";

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info),
      last_file_handle_(0),
      weak_ptr_factory_(this) {
}

FakeProvidedFileSystem::~FakeProvidedFileSystem() {}

void FakeProvidedFileSystem::RequestUnmount(
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::File::FILE_OK));
}

void FakeProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    const ProvidedFileSystemInterface::GetMetadataCallback& callback) {
  if (entry_path.AsUTF8Unsafe() == "/") {
    EntryMetadata metadata;
    metadata.size = 0;
    metadata.is_directory = true;
    base::Time modification_time;
    const bool result = base::Time::FromString("Thu Apr 24 00:46:52 UTC 2014",
                                               &modification_time);
    DCHECK(result);
    metadata.modification_time = modification_time;

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, metadata, base::File::FILE_OK));
    return;
  }

  if (entry_path.AsUTF8Unsafe() == kFakeFilePath) {
    EntryMetadata metadata;
    metadata.size = kFakeFileSize;
    metadata.is_directory = false;
    base::Time modification_time;
    const bool result =
        base::Time::FromString(kFakeFileModificationTime, &modification_time);
    DCHECK(result);
    metadata.modification_time = modification_time;
    metadata.mime_type = "text/plain";

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, metadata, base::File::FILE_OK));
    return;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, EntryMetadata(), base::File::FILE_ERROR_NOT_FOUND));
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
                      kFakeFileName,
                      fileapi::DirectoryEntry::FILE,
                      kFakeFileSize,
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
                                      const OpenFileCallback& callback) {
  if (mode == OPEN_FILE_MODE_WRITE) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   0 /* file_handle */,
                   base::File::FILE_ERROR_ACCESS_DENIED));
  }

  if (file_path.AsUTF8Unsafe() != "/hello.txt") {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, 0 /* file_handle */, base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  const int file_handle = ++last_file_handle_;
  opened_files_[file_handle] = file_path;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, file_handle, base::File::FILE_OK));
}

void FakeProvidedFileSystem::CloseFile(
    int file_handle,
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  const OpenedFilesMap::iterator opened_file_it =
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
  const OpenedFilesMap::iterator opened_file_it =
      opened_files_.find(file_handle);
  if (opened_file_it == opened_files_.end() ||
      opened_file_it->second.AsUTF8Unsafe() != kFakeFilePath) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   0 /* chunk_length */,
                   false /* has_more */,
                   base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  // Send the response byte by byte.
  size_t current_offset = static_cast<size_t>(offset);
  size_t current_length = static_cast<size_t>(length);

  // Reading behind EOF is fine, it will just return 0 bytes.
  if (current_offset >= kFakeFileSize || !current_length) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   0 /* chunk_length */,
                   false /* has_more */,
                   base::File::FILE_OK));
  }

  while (current_offset < kFakeFileSize && current_length) {
    buffer->data()[current_offset - offset] = kFakeFileText[current_offset];
    const bool has_more =
        (current_offset + 1 < kFakeFileSize) && (current_length - 1);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, 1 /* chunk_length */, has_more, base::File::FILE_OK));
    current_offset++;
    current_length--;
  }
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
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  return new FakeProvidedFileSystem(file_system_info);
}

base::WeakPtr<ProvidedFileSystemInterface>
FakeProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace file_system_provider
}  // namespace chromeos
