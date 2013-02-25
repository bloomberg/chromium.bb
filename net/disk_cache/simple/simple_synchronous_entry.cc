// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_synchronous_entry.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using base::ClosePlatformFile;
using base::FilePath;
using base::GetPlatformFileInfo;
using base::PlatformFileError;
using base::PlatformFileInfo;
using base::PLATFORM_FILE_CREATE;
using base::PLATFORM_FILE_OK;
using base::PLATFORM_FILE_OPEN;
using base::PLATFORM_FILE_READ;
using base::PLATFORM_FILE_WRITE;
using base::ReadPlatformFile;
using base::TaskRunner;
using base::Time;
using base::TruncatePlatformFile;
using base::WritePlatformFile;

namespace {

std::string GetFilenameForKeyAndIndex(const std::string& key, int index) {
  const std::string sha_hash = base::SHA1HashString(key);
  return StringPrintf("%02x%02x%02x%02x%02x_%1d",
                      implicit_cast<unsigned char>(sha_hash[0]),
                      implicit_cast<unsigned char>(sha_hash[1]),
                      implicit_cast<unsigned char>(sha_hash[2]),
                      implicit_cast<unsigned char>(sha_hash[3]),
                      implicit_cast<unsigned char>(sha_hash[4]), index);
}

int32 DataSizeFromKeyAndFileSize(size_t key_size, int64 file_size) {
  int64 data_size = file_size - key_size - sizeof(disk_cache::SimpleFileHeader);
  DCHECK_GE(implicit_cast<int64>(std::numeric_limits<int32>::max()), data_size);
  return data_size;
}

int64 FileOffsetFromDataOffset(size_t key_size, int data_offset) {
  const int64 headers_size = sizeof(disk_cache::SimpleFileHeader) +
                             key_size;
  return headers_size + data_offset;
}

}  // namespace

namespace disk_cache {

// static
void SimpleSynchronousEntry::OpenEntry(
    const FilePath& path,
    const std::string& key,
    const scoped_refptr<TaskRunner>& callback_runner,
    const SynchronousCreationCallback& callback) {
  SimpleSynchronousEntry* sync_entry =
      new SimpleSynchronousEntry(callback_runner, path, key);

  if (!sync_entry->InitializeForOpen()) {
    delete sync_entry;
    sync_entry = NULL;
  }
  callback_runner->PostTask(FROM_HERE, base::Bind(callback, sync_entry));
}

// static
void SimpleSynchronousEntry::CreateEntry(
    const FilePath& path,
    const std::string& key,
    const scoped_refptr<TaskRunner>& callback_runner,
    const SynchronousCreationCallback& callback) {
  SimpleSynchronousEntry* sync_entry =
      new SimpleSynchronousEntry(callback_runner, path, key);

  if (!sync_entry->InitializeForCreate()) {
    delete sync_entry;
    sync_entry = NULL;
  }
  callback_runner->PostTask(FROM_HERE, base::Bind(callback, sync_entry));
}

// static
void SimpleSynchronousEntry::DoomEntry(
    const FilePath& path,
    const std::string& key,
    scoped_refptr<TaskRunner> callback_runner,
    const net::CompletionCallback& callback) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    FilePath to_delete = path.AppendASCII(GetFilenameForKeyAndIndex(key, i));
    bool ALLOW_UNUSED result = file_util::Delete(to_delete, false);
    DLOG_IF(ERROR, !result) << "Could not delete " << to_delete.MaybeAsASCII();
  }
  if (!callback.is_null())
    callback_runner->PostTask(FROM_HERE, base::Bind(callback, net::OK));
}

void SimpleSynchronousEntry::DoomAndClose() {
  scoped_refptr<TaskRunner> callback_runner = callback_runner_;
  FilePath path = path_;
  std::string key = key_;

  Close();
  // |this| is now deleted.

  DoomEntry(path, key, callback_runner, net::CompletionCallback());
}

void SimpleSynchronousEntry::Close() {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    bool ALLOW_UNUSED result = ClosePlatformFile(files_[i]);
    DLOG_IF(INFO, !result) << "Could not Close() file.";
  }
  delete this;
}

void SimpleSynchronousEntry::ReadData(
    int index,
    int offset,
    net::IOBuffer* buf,
    int buf_len,
    const SynchronousOperationCallback& callback) {
  DCHECK(initialized_);

  int64 file_offset = FileOffsetFromDataOffset(key_.size(), offset);
  int bytes_read = ReadPlatformFile(files_[index], file_offset,
                                    buf->data(), buf_len);
  if (bytes_read > 0)
    last_used_ = Time::Now();
  int result = (bytes_read >= 0) ? bytes_read : net::ERR_FAILED;
  callback_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void SimpleSynchronousEntry::WriteData(
    int index,
    int offset,
    net::IOBuffer* buf,
    int buf_len,
    const SynchronousOperationCallback& callback,
    bool truncate) {
  DCHECK(initialized_);

  int64 file_offset = FileOffsetFromDataOffset(key_.size(), offset);
  if (buf_len > 0) {
    if (WritePlatformFile(files_[index], file_offset, buf->data(), buf_len) !=
        buf_len) {
      callback_runner_->PostTask(FROM_HERE,
                                 base::Bind(callback, net::ERR_FAILED));
      return;
    }
    data_size_[index] = std::max(data_size_[index], offset + buf_len);
  }
  if (truncate) {
    data_size_[index] = offset + buf_len;
    if (!TruncatePlatformFile(files_[index], file_offset + buf_len)) {
      callback_runner_->PostTask(FROM_HERE,
                                 base::Bind(callback, net::ERR_FAILED));
      return;
    }
  }
  last_modified_ = Time::Now();
  callback_runner_->PostTask(FROM_HERE, base::Bind(callback, buf_len));
}

SimpleSynchronousEntry::SimpleSynchronousEntry(
    const scoped_refptr<TaskRunner>& callback_runner,
    const FilePath& path,
    const std::string& key)
    : callback_runner_(callback_runner),
      path_(path),
      key_(key),
      initialized_(false) {
}

SimpleSynchronousEntry::~SimpleSynchronousEntry() {
}

bool SimpleSynchronousEntry::OpenOrCreateFiles(bool create) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    FilePath filename = path_.AppendASCII(GetFilenameForKeyAndIndex(key_, i));
    int flags = PLATFORM_FILE_READ | PLATFORM_FILE_WRITE;
    if (create)
      flags |= PLATFORM_FILE_CREATE;
    else
      flags |= PLATFORM_FILE_OPEN;
    PlatformFileError error;
    files_[i] = CreatePlatformFile(filename, flags, NULL, &error);
    if (error != PLATFORM_FILE_OK) {
      DVLOG(8) << "CreatePlatformFile error " << error << " while "
               << (create ? "creating " : "opening ")
               << filename.MaybeAsASCII();
      while (--i >= 0) {
        bool ALLOW_UNUSED did_close = ClosePlatformFile(files_[i]);
        DLOG_IF(INFO, !did_close) << "Could not close file "
                                  << filename.MaybeAsASCII();
      }
      return false;
    }
  }

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    PlatformFileInfo file_info;
    bool success = GetPlatformFileInfo(files_[i], &file_info);
    if (!success) {
      DLOG(WARNING) << "Could not get platform file info.";
      continue;
    }
    last_used_ = std::max(last_used_, file_info.last_accessed);
    last_modified_ = std::max(last_modified_, file_info.last_modified);
    data_size_[i] = DataSizeFromKeyAndFileSize(key_.size(), file_info.size);
  }

  return true;
}

bool SimpleSynchronousEntry::InitializeForOpen() {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(false))
    return false;

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    SimpleFileHeader header;
    int header_read_result =
        ReadPlatformFile(files_[i], 0, reinterpret_cast<char*>(&header),
                         sizeof(header));
    if (header_read_result != sizeof(header)) {
      DLOG(WARNING) << "Cannot read header from entry.";
      return false;
    }

    if (header.initial_magic_number != kSimpleInitialMagicNumber) {
      // TODO(gavinp): This seems very bad; for now we log at WARNING, but we
      // should give consideration to not saturating the log with these if that
      // becomes a problem.
      DLOG(WARNING) << "Magic number did not match.";
      return false;
    }

    if (header.version != kSimpleVersion) {
      DLOG(WARNING) << "Unreadable version.";
      return false;
    }

    scoped_ptr<char[]> key(new char[header.key_length]);
    int key_read_result = ReadPlatformFile(files_[i], sizeof(header),
                                           key.get(), header.key_length);
    if (key_read_result != implicit_cast<int>(header.key_length)) {
      DLOG(WARNING) << "Cannot read key from entry.";
      return false;
    }
    if (header.key_length != key_.size() ||
        std::memcmp(key_.data(), key.get(), key_.size()) != 0) {
      // TODO(gavinp): Since the way we use Entry SHA to name entries means this
      // is expected to occur at some frequency, add unit_tests that this does
      // is handled gracefully at higher levels.
      DLOG(WARNING) << "Key mismatch on open.";
      return false;
    }

    if (base::Hash(key.get(), header.key_length) != header.key_hash) {
      DLOG(WARNING) << "Hash mismatch on key.";
      return false;
    }
  }

  initialized_ = true;
  return true;
}

bool SimpleSynchronousEntry::InitializeForCreate() {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(true)) {
    DLOG(WARNING) << "Could not create platform files.";
    return false;
  }

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    SimpleFileHeader header;
    header.initial_magic_number = kSimpleInitialMagicNumber;
    header.version = kSimpleVersion;

    header.key_length = key_.size();
    header.key_hash = base::Hash(key_);

    if (WritePlatformFile(files_[i], 0, reinterpret_cast<char*>(&header),
                          sizeof(header)) != sizeof(header)) {
      // TODO(gavinp): Clean up created files.
      DLOG(WARNING) << "Could not write headers to new cache entry.";
      return false;
    }

    if (WritePlatformFile(files_[i], sizeof(header), key_.data(),
                          key_.size()) != implicit_cast<int>(key_.size())) {
      // TODO(gavinp): Clean up created files.
      DLOG(WARNING) << "Could not write keys to new cache entry.";
      return false;
    }
  }

  initialized_ = true;
  return true;
}

}  // namespace disk_cache
