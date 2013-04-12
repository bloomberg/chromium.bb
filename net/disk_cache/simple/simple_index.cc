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

class FileAutoCloser {
 public:
  explicit FileAutoCloser(const base::PlatformFile& file) : file_(file) { }
  ~FileAutoCloser() {
    base::ClosePlatformFile(file_);
  }
 private:
  base::PlatformFile file_;
  DISALLOW_COPY_AND_ASSIGN(FileAutoCloser);
};

}  // namespace

namespace disk_cache {

SimpleIndex::SimpleIndex(
    const scoped_refptr<base::TaskRunner>& cache_thread,
    const scoped_refptr<base::TaskRunner>& io_thread,
    const base::FilePath& path)
    : cache_size_(0),
      initialized_(false),
      index_filename_(path.AppendASCII("simple-index")),
      cache_thread_(cache_thread),
      io_thread_(io_thread) {}

SimpleIndex::~SimpleIndex() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
}

void SimpleIndex::Initialize() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  MergeCallback merge_callback = base::Bind(&SimpleIndex::MergeInitializingSet,
                                            this->AsWeakPtr());
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&SimpleIndex::LoadFromDisk,
                                        index_filename_,
                                        io_thread_,
                                        merge_callback),
                             true);
}

// static
void SimpleIndex::LoadFromDisk(
    const base::FilePath& index_filename,
    const scoped_refptr<base::TaskRunner>& io_thread,
    const MergeCallback& merge_callback) {
  // Open the index file.
  base::PlatformFileError error;
  base::PlatformFile index_file = base::CreatePlatformFile(
      index_filename,
      base::PLATFORM_FILE_OPEN_ALWAYS |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE,
      NULL,
      &error);
  FileAutoCloser auto_close_index_file(index_file);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Error opening file " << index_filename.value();
    return RestoreFromDisk(index_filename, io_thread, merge_callback);
  }

  uLong incremental_crc = crc32(0L, Z_NULL, 0);
  int64 index_file_offset = 0;
  SimpleIndexFile::Header header;
  if (base::ReadPlatformFile(index_file,
                             index_file_offset,
                             reinterpret_cast<char*>(&header),
                             sizeof(header)) != sizeof(header)) {
    return RestoreFromDisk(index_filename, io_thread, merge_callback);
  }
  index_file_offset += sizeof(header);
  incremental_crc = crc32(incremental_crc,
                          reinterpret_cast<const Bytef*>(&header),
                          implicit_cast<uInt>(sizeof(header)));

  if (!CheckHeader(header)) {
    LOG(ERROR) << "Invalid header on Simple Cache Index.";
    return RestoreFromDisk(index_filename, io_thread, merge_callback);
  }

  const int entries_buffer_size =
      header.number_of_entries * SimpleIndexFile::kEntryMetadataSize;

  scoped_ptr<char[]> entries_buffer(new char[entries_buffer_size]);
  if (base::ReadPlatformFile(index_file,
                             index_file_offset,
                             entries_buffer.get(),
                             entries_buffer_size) != entries_buffer_size) {
    return RestoreFromDisk(index_filename, io_thread, merge_callback);
  }
  index_file_offset += entries_buffer_size;
  incremental_crc = crc32(incremental_crc,
                          reinterpret_cast<const Bytef*>(entries_buffer.get()),
                          implicit_cast<uInt>(entries_buffer_size));

  SimpleIndexFile::Footer footer;
  if (base::ReadPlatformFile(index_file,
                             index_file_offset,
                             reinterpret_cast<char*>(&footer),
                             sizeof(footer)) != sizeof(footer)) {
    return RestoreFromDisk(index_filename, io_thread, merge_callback);
  }
  const uint32 crc_read = footer.crc;
  const uint32 crc_calculated = incremental_crc;
  if (crc_read != crc_calculated)
    return RestoreFromDisk(index_filename, io_thread, merge_callback);

  scoped_ptr<EntrySet> index_file_entries(new EntrySet());
  int entries_buffer_offset = 0;
  while(entries_buffer_offset < entries_buffer_size) {
    SimpleIndexFile::EntryMetadata entry_metadata;
    SimpleIndexFile::EntryMetadata::DeSerialize(
        &entries_buffer.get()[entries_buffer_offset], &entry_metadata);
    InsertInternal(index_file_entries.get(), entry_metadata);
    entries_buffer_offset += SimpleIndexFile::kEntryMetadataSize;
  }
  DCHECK_EQ(header.number_of_entries, index_file_entries->size());

  io_thread->PostTask(FROM_HERE,
                      base::Bind(merge_callback,
                                 base::Passed(&index_file_entries)));
}

void SimpleIndex::Insert(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // Upon insert we don't know yet the size of the entry.
  // It will be updated later when the SimpleEntryImpl finishes opening or
  // creating the new entry, and then UpdateEntrySize will be called.
  const std::string hash_key = GetEntryHashForKey(key);
  InsertInternal(&entries_set_, SimpleIndexFile::EntryMetadata(
      hash_key,
      base::Time::Now(), 0));
  if (!initialized_)
    removed_entries_.erase(hash_key);
}

void SimpleIndex::Remove(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  UpdateEntrySize(key, 0);
  const std::string hash_key = GetEntryHashForKey(key);
  entries_set_.erase(hash_key);

  if (!initialized_)
    removed_entries_.insert(hash_key);
}

bool SimpleIndex::Has(const std::string& key) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // If not initialized, always return true, forcing it to go to the disk.
  return !initialized_ || entries_set_.count(GetEntryHashForKey(key)) != 0;
}

bool SimpleIndex::UseIfExists(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // Always update the last used time, even if it is during initialization.
  // It will be merged later.
  EntrySet::iterator it = entries_set_.find(GetEntryHashForKey(key));
  if (it == entries_set_.end())
    // If not initialized, always return true, forcing it to go to the disk.
    return !initialized_;
  it->second.SetLastUsedTime(base::Time::Now());
  return true;
}

bool SimpleIndex::UpdateEntrySize(const std::string& key, uint64 entry_size) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  EntrySet::iterator it = entries_set_.find(GetEntryHashForKey(key));
  if (it == entries_set_.end())
    return false;

  // Update the total cache size with the new entry size.
  cache_size_ -= it->second.entry_size;
  cache_size_ += entry_size;
  it->second.entry_size = entry_size;

  return true;
}

// static
void SimpleIndex::InsertInternal(
    EntrySet* entry_set,
    const SimpleIndexFile::EntryMetadata& entry_metadata) {
  // TODO(felipeg): Use a hash_set instead of a hash_map.
  DCHECK(entry_set);
  entry_set->insert(make_pair(entry_metadata.GetHashKey(), entry_metadata));
}

// static
void SimpleIndex::RestoreFromDisk(
    const base::FilePath& index_filename,
    const scoped_refptr<base::TaskRunner>& io_thread,
    const MergeCallback& merge_callback) {
  using file_util::FileEnumerator;
  LOG(INFO) << "Simple Cache Index is being restored from disk.";

  file_util::Delete(index_filename, /* recursive = */ false);
  scoped_ptr<EntrySet> index_file_entries(new EntrySet());

  // TODO(felipeg,gavinp): Fix this once we have a one-file per entry format.
  COMPILE_ASSERT(kSimpleEntryFileCount == 3,
                 file_pattern_must_match_file_count);
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*_[0-2]");
  FileEnumerator enumerator(index_filename.DirName(),
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

    int64 file_size = FileEnumerator::GetFilesize(find_info);
    EntrySet::iterator it = index_file_entries->find(hash_key);
    if (it == index_file_entries->end()) {
      InsertInternal(index_file_entries.get(), SimpleIndexFile::EntryMetadata(
          hash_key, last_used_time, file_size));
    } else {
      // Summing up the total size of the entry through all the *_[0-2] files
      it->second.entry_size += file_size;
    }
  }

  io_thread->PostTask(FROM_HERE,
                      base::Bind(merge_callback,
                                 base::Passed(&index_file_entries)));
}

void SimpleIndex::MergeInitializingSet(
    scoped_ptr<EntrySet> index_file_entries) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // First, remove the entries that are in the |removed_entries_| from both
  // sets.
  for (base::hash_set<std::string>::const_iterator it =
           removed_entries_.begin(); it != removed_entries_.end(); ++it) {
    entries_set_.erase(*it);
    index_file_entries->erase(*it);
  }

  // Recalculate the cache size while merging the two sets.
  cache_size_ = 0;
  for (EntrySet::const_iterator it = index_file_entries->begin();
       it != index_file_entries->end(); ++it) {
    // If there is already an entry in the current entries_set_, we need to
    // merge the new data there with the data loaded in the initialization.
    EntrySet::iterator current_entry = entries_set_.find(it->first);
    if (current_entry != entries_set_.end()) {
      // When Merging, existing valid data in the |current_entry| will prevail.
      SimpleIndexFile::EntryMetadata::Merge(
          it->second, &(current_entry->second));
      cache_size_ += current_entry->second.entry_size;
    } else {
      InsertInternal(&entries_set_, it->second);
      cache_size_ += it->second.entry_size;
    }
  }

  initialized_ = true;
}

void SimpleIndex::Serialize(std::string* out_buffer) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
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

void SimpleIndex::WriteToDisk() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  scoped_ptr<std::string> buffer(new std::string());
  Serialize(buffer.get());
  cache_thread_->PostTask(FROM_HERE, base::Bind(
      &SimpleIndex::UpdateFile,
      index_filename_,
      index_filename_.DirName().AppendASCII("index_temp"),
      base::Passed(&buffer)));
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
