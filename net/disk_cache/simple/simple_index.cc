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
#include "third_party/zlib/zlib.h"

namespace {

const uint64 kMaxEntiresInIndex = 100000000;

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

bool CheckMetadata(disk_cache::SimpleIndexFile::Header metadata) {
  return metadata.size <= kMaxEntiresInIndex &&
      metadata.initial_magic_number == disk_cache::kSimpleInitialMagicNumber &&
      metadata.version == disk_cache::kSimpleVersion;
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
  // TODO(felipeg): If loading the index file fails, we should list all the
  // files in the directory.

  uLong incremental_crc = crc32(0L, Z_NULL, 0);

  int64 index_file_offset = 0;
  SimpleIndexFile::Header metadata;
  if (base::ReadPlatformFile(index_file_,
                             index_file_offset,
                             reinterpret_cast<char*>(&metadata),
                             sizeof(metadata)) != sizeof(metadata)) {
    CloseIndexFile();
    return false;
  }
  index_file_offset += sizeof(metadata);
  incremental_crc = crc32(incremental_crc,
                          reinterpret_cast<const Bytef*>(&metadata),
                          implicit_cast<uInt>(sizeof(metadata)));

  if (!CheckMetadata(metadata)) {
    LOG(ERROR) << "Invalid metadata on Simple Cache Index.";
    CloseIndexFile();
    return false;
  }

  char hash_key[kEntryHashKeySize];
  while(entries_set_.size() < metadata.size) {
    // TODO(felipeg): Read things in larger chunks/optimize this.
    if (base::ReadPlatformFile(index_file_,
                               index_file_offset,
                               hash_key,
                               kEntryHashKeySize) != kEntryHashKeySize) {
      CloseIndexFile();
      return false;
    }
    index_file_offset += kEntryHashKeySize;
    incremental_crc = crc32(incremental_crc,
                            reinterpret_cast<const Bytef*>(hash_key),
                            implicit_cast<uInt>(kEntryHashKeySize));
    entries_set_.insert(std::string(hash_key, kEntryHashKeySize));
  }

  SimpleIndexFile::Footer footer;
  if (base::ReadPlatformFile(index_file_,
                             index_file_offset,
                             reinterpret_cast<char*>(&footer),
                             sizeof(footer)) != sizeof(footer)) {
    CloseIndexFile();
    return false;
  }
  const uint32 crc_read = footer.crc;
  const uint32 crc_calculated = incremental_crc;
  if (crc_read != crc_calculated) {
    DCHECK_EQ(crc_read, crc_calculated);
    CloseIndexFile();
    return false;
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

void SimpleIndex::Serialize(std::string* out_buffer) {
  DCHECK(out_buffer);
  SimpleIndexFile::Header metadata;
  SimpleIndexFile::Footer footer;

  metadata.initial_magic_number = kSimpleInitialMagicNumber;
  metadata.version = kSimpleVersion;
  metadata.size = entries_set_.size();

  out_buffer->reserve(sizeof(metadata) +
                      kEntryHashKeySize * entries_set_.size() +
                      sizeof(footer));

  // Metadata goes first.
  out_buffer->append(reinterpret_cast<const char*>(&metadata),
                     sizeof(metadata));

  // Then all the hashes in the entries_set.
  for (EntrySet::const_iterator it = entries_set_.begin();
       it != entries_set_.end(); ++it) {
    out_buffer->append(*it);
  }

  // Then, CRC.
  footer.crc = crc32(crc32(0, Z_NULL, 0),
                     reinterpret_cast<const Bytef*>(out_buffer->data()),
                     implicit_cast<uInt>(out_buffer->size()));

  out_buffer->append(reinterpret_cast<const char*>(&footer), sizeof(footer));
}

void SimpleIndex::Cleanup() {
  scoped_ptr<std::string> buffer(new std::string());
  Serialize(buffer.get());
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
