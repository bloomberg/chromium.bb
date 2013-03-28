// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/task_runner.h"
#include "base/threading/worker_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_disk_format.h"


namespace {

bool WriteHashKeyToFile(const base::PlatformFile& file,
                        size_t offset,
                        const std::string& hash_key) {
  // Append the hash_key to the index file.
  if (base::WritePlatformFile(file, offset, hash_key.data(), hash_key.size()) !=
      implicit_cast<int>(hash_key.size())) {
    LOG(ERROR) << "Failed to write index file.";
    return false;
  }
  return true;
}

}  // namespace

namespace disk_cache {

SimpleIndex::SimpleIndex(
    const scoped_refptr<base::TaskRunner>& cache_thread,
    const base::FilePath& path)
    : path_(path),
      cache_thread_(cache_thread) {
  index_filename_ = path_.AppendASCII("simple-index");
}

bool SimpleIndex::Initialize() {
  if (!OpenIndexFile())
    return false;
  // TODO(felipeg): Add a checksum to the index file.
  // TODO(felipeg): If loading the index file fails, we should list all the
  // files in the directory.
  // Both TODOs above should be fixed before the field experiment.
  char hash_key[kEntryHashKeySize];
  int index_file_offset = 0;
  while(base::ReadPlatformFile(index_file_,
                               index_file_offset,
                               hash_key,
                               kEntryHashKeySize) == kEntryHashKeySize) {
    index_file_offset += kEntryHashKeySize;
    entries_set_.insert(std::string(hash_key, kEntryHashKeySize));
  }

  CloseIndexFile();
  return true;
}

void SimpleIndex::Insert(const std::string& key) {
  std::string hash_key = GetEntryHashForKey(key);
  entries_set_.insert(hash_key);
}

void SimpleIndex::Remove(const std::string& key) {
  entries_set_.erase(GetEntryHashForKey(key));
}

bool SimpleIndex::Has(const std::string& key) const {
  return entries_set_.count(GetEntryHashForKey(key)) != 0;
}

void SimpleIndex::Cleanup() {
  scoped_ptr<std::string> buffer(new std::string());
  buffer->reserve(kEntryHashKeySize * entries_set_.size());
  for (EntrySet::const_iterator it = entries_set_.begin();
       it != entries_set_.end(); ++it) {
    buffer->append(*it);
  }
  cache_thread_->PostTask(FROM_HERE,
                            base::Bind(&SimpleIndex::UpdateFile,
                                       index_filename_,
                                       path_.AppendASCII("index_temp"),
                                       base::Passed(&buffer)));
}

SimpleIndex::~SimpleIndex() {
  CloseIndexFile();
}

bool SimpleIndex::OpenIndexFile() {
  base::PlatformFileError error;
  index_file_ = base::CreatePlatformFile(index_filename_,
                                         base::PLATFORM_FILE_OPEN_ALWAYS |
                                         base::PLATFORM_FILE_READ |
                                         base::PLATFORM_FILE_WRITE,
                                         NULL,
                                         &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Error opening file " << index_filename_.value();
    return false;
  }
  return true;
}

bool SimpleIndex::CloseIndexFile() {
  return base::ClosePlatformFile(index_file_);
}

// static
void SimpleIndex::UpdateFile(const base::FilePath& index_filename,
                             const base::FilePath& temp_filename,
                             scoped_ptr<std::string> buffer) {
  // TODO(felipeg): Add a checksum to the file.
  int bytes_written = file_util::WriteFile(
      temp_filename, buffer->data(), buffer->size());
  DCHECK_EQ(bytes_written, implicit_cast<int>(buffer->size()));
  if (bytes_written != static_cast<int>(buffer->size())) {
    // TODO(felipeg): Add better error handling.
    LOG(ERROR) << "Could not write Simple Cache index to temporary file: "
               << temp_filename.value();
    file_util::Delete(temp_filename, /* recursive = */ false);
    return;
  }

  // Swap temp and index_file.
  bool result = file_util::ReplaceFile(temp_filename, index_filename);
  DCHECK(result);
}

}  // namespace disk_cache
