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

bool CheckHeader(disk_cache::SimpleIndexFile::Header header) {
  return header.number_of_entries <= kMaxEntiresInIndex &&
      header.initial_magic_number ==
      disk_cache::kSimpleIndexInitialMagicNumber &&
      header.version == disk_cache::kSimpleVersion;
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
    return RestoreFromDisk();
  uLong incremental_crc = crc32(0L, Z_NULL, 0);
  int64 index_file_offset = 0;
  SimpleIndexFile::Header header;
  if (base::ReadPlatformFile(index_file_,
                             index_file_offset,
                             reinterpret_cast<char*>(&header),
                             sizeof(header)) != sizeof(header)) {
    return RestoreFromDisk();
  }
  index_file_offset += sizeof(header);
  incremental_crc = crc32(incremental_crc,
                          reinterpret_cast<const Bytef*>(&header),
                          implicit_cast<uInt>(sizeof(header)));

  if (!CheckHeader(header)) {
    LOG(ERROR) << "Invalid header on Simple Cache Index.";
    return RestoreFromDisk();
  }

  const int entries_buffer_size =
      header.number_of_entries * SimpleIndexFile::kEntryMetadataSize;

  scoped_ptr<char[]> entries_buffer(new char[entries_buffer_size]);
  if (base::ReadPlatformFile(index_file_,
                             index_file_offset,
                             entries_buffer.get(),
                             entries_buffer_size) != entries_buffer_size) {
    return RestoreFromDisk();
  }
  index_file_offset += entries_buffer_size;
  incremental_crc = crc32(incremental_crc,
                          reinterpret_cast<const Bytef*>(entries_buffer.get()),
                          implicit_cast<uInt>(entries_buffer_size));

  SimpleIndexFile::Footer footer;
  if (base::ReadPlatformFile(index_file_,
                             index_file_offset,
                             reinterpret_cast<char*>(&footer),
                             sizeof(footer)) != sizeof(footer)) {
    return RestoreFromDisk();
  }
  const uint32 crc_read = footer.crc;
  const uint32 crc_calculated = incremental_crc;
  if (crc_read != crc_calculated)
    return RestoreFromDisk();

  int entries_buffer_offset = 0;
  while(entries_buffer_offset < entries_buffer_size) {
    SimpleIndexFile::EntryMetadata entry_metadata;
    SimpleIndexFile::EntryMetadata::DeSerialize(
        &entries_buffer.get()[entries_buffer_offset], &entry_metadata);
    InsertInternal(entry_metadata);
    entries_buffer_offset += SimpleIndexFile::kEntryMetadataSize;
  }
  DCHECK_EQ(header.number_of_entries, entries_set_.size());
  CloseIndexFile();
  return true;
}

void SimpleIndex::Insert(const std::string& key) {
  InsertInternal(SimpleIndexFile::EntryMetadata(GetEntryHashForKey(key),
                                                base::Time::Now()));
}

void SimpleIndex::Remove(const std::string& key) {
  entries_set_.erase(GetEntryHashForKey(key));
}

bool SimpleIndex::Has(const std::string& key) const {
  return entries_set_.count(GetEntryHashForKey(key)) != 0;
}

bool SimpleIndex::UseIfExists(const std::string& key) {
  EntrySet::iterator it = entries_set_.find(GetEntryHashForKey(key));
  if (it == entries_set_.end())
    return false;
  it->second.SetLastUsedTime(base::Time::Now());
  return true;
}

void SimpleIndex::InsertInternal(
    const SimpleIndexFile::EntryMetadata& entry_metadata) {
  entries_set_.insert(make_pair(entry_metadata.GetHashKey(), entry_metadata));
}

bool SimpleIndex::RestoreFromDisk() {
  using file_util::FileEnumerator;
  LOG(INFO) << "Simple Cache Index is being restored from disk.";
  CloseIndexFile();
  file_util::Delete(index_filename_, /* recursive = */ false);
  entries_set_.clear();
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*_0");
  FileEnumerator enumerator(path_,
                            false /* recursive */,
                            FileEnumerator::FILES,
                            file_pattern);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    const base::FilePath::StringType base_name = file_path.BaseName().value();
    // Converting to std::string is OK since we never use UTF8 wide chars in our
    // file names.
    const std::string hash_name(base_name.begin(), base_name.end());
    const std::string hash_key = hash_name.substr(0, kEntryHashKeySize);

    FileEnumerator::FindInfo find_info = {};
    enumerator.GetFindInfo(&find_info);
    base::Time last_used_time;
#if defined(OS_POSIX)
    // For POSIX systems, a last access time is available. However, it's not
    // guaranteed to be more accurate than mtime. It is no worse though.
    last_used_time = base::Time::FromTimeT(find_info.stat.st_atime);
#endif
    if (last_used_time.is_null())
      last_used_time = FileEnumerator::GetLastModifiedTime(find_info);
    InsertInternal(SimpleIndexFile::EntryMetadata(hash_key, last_used_time));
  }
  // TODO(felipeg): Detect unrecoverable problems and return false here.
  return true;
}

void SimpleIndex::Serialize(std::string* out_buffer) {
  DCHECK(out_buffer);
  SimpleIndexFile::Header header;
  SimpleIndexFile::Footer footer;

  header.initial_magic_number = kSimpleIndexInitialMagicNumber;
  header.version = kSimpleVersion;
  header.number_of_entries = entries_set_.size();

  out_buffer->reserve(sizeof(header) +
                      kEntryHashKeySize * entries_set_.size() +
                      sizeof(footer));

  // The Header goes first.
  out_buffer->append(reinterpret_cast<const char*>(&header),
                     sizeof(header));

  // Then all the entries from |entries_set_|.
  for (EntrySet::const_iterator it = entries_set_.begin();
       it != entries_set_.end(); ++it) {
    SimpleIndexFile::EntryMetadata::Serialize(it->second, out_buffer);
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
