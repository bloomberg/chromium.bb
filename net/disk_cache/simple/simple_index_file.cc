// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index_file.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"


namespace {

const uint64 kMaxEntiresInIndex = 100000000;

uint32 CalculatePickleCRC(const Pickle& pickle) {
  return crc32(crc32(0, Z_NULL, 0),
               reinterpret_cast<const Bytef*>(pickle.payload()),
               pickle.payload_size());
}

void DoomEntrySetReply(scoped_ptr<int> result,
                       const base::Callback<void(int)>& reply_callback) {
  reply_callback.Run(*result.get());
}

void WriteToDiskInternal(const base::FilePath& index_filename,
                         scoped_ptr<Pickle> pickle,
                         const base::TimeTicks& start_time,
                         bool app_on_background) {
  const base::FilePath temp_filename =
      index_filename.DirName().AppendASCII("index_temp");
  int bytes_written = file_util::WriteFile(
      temp_filename,
      reinterpret_cast<const char*>(pickle->data()),
      pickle->size());
  DCHECK_EQ(bytes_written, implicit_cast<int>(pickle->size()));
  if (bytes_written != static_cast<int>(pickle->size())) {
    // TODO(felipeg): Add better error handling.
    LOG(ERROR) << "Could not write Simple Cache index to temporary file: "
               << temp_filename.value();
    file_util::Delete(temp_filename, /* recursive = */ false);
  } else {
    // Swap temp and index_file.
    bool result = file_util::ReplaceFile(temp_filename, index_filename);
    DCHECK(result);
  }
  if (app_on_background) {
    UMA_HISTOGRAM_TIMES("SimpleCache.IndexWriteToDiskTime.Background",
                        (base::TimeTicks::Now() - start_time));
  } else {
    UMA_HISTOGRAM_TIMES("SimpleCache.IndexWriteToDiskTime.Foreground",
                        (base::TimeTicks::Now() - start_time));
  }
}

}  // namespace

namespace disk_cache {

// static
const char SimpleIndexFile::kIndexFileName[] = "the-real-index";

SimpleIndexFile::IndexMetadata::IndexMetadata() :
    magic_number_(kSimpleIndexMagicNumber),
    version_(kSimpleVersion),
    number_of_entries_(0),
    cache_size_(0) {}

SimpleIndexFile::IndexMetadata::IndexMetadata(
    uint64 number_of_entries, uint64 cache_size) :
    magic_number_(kSimpleIndexMagicNumber),
    version_(kSimpleVersion),
    number_of_entries_(number_of_entries),
    cache_size_(cache_size) {}

void SimpleIndexFile::IndexMetadata::Serialize(Pickle* pickle) const {
  DCHECK(pickle);
  pickle->WriteUInt64(magic_number_);
  pickle->WriteUInt32(version_);
  pickle->WriteUInt64(number_of_entries_);
  pickle->WriteUInt64(cache_size_);
}

bool SimpleIndexFile::IndexMetadata::Deserialize(PickleIterator* it) {
  DCHECK(it);
  return it->ReadUInt64(&magic_number_) &&
      it->ReadUInt32(&version_) &&
      it->ReadUInt64(&number_of_entries_)&&
      it->ReadUInt64(&cache_size_);
}

bool SimpleIndexFile::IndexMetadata::CheckIndexMetadata() {
  return number_of_entries_ <= kMaxEntiresInIndex &&
      magic_number_ == disk_cache::kSimpleIndexMagicNumber &&
      version_ == disk_cache::kSimpleVersion;
}

SimpleIndexFile::SimpleIndexFile(
    base::SingleThreadTaskRunner* cache_thread,
    base::TaskRunner* worker_pool,
    const base::FilePath& index_file_directory)
    : cache_thread_(cache_thread),
      worker_pool_(worker_pool),
      index_file_path_(index_file_directory.AppendASCII(kIndexFileName)) {
}

SimpleIndexFile::~SimpleIndexFile() {}

void SimpleIndexFile::LoadIndexEntries(
    scoped_refptr<base::SingleThreadTaskRunner> response_thread,
    const IndexCompletionCallback& completion_callback) {
  worker_pool_->PostTask(
      FROM_HERE,
      base::Bind(&SimpleIndexFile::LoadIndexEntriesInternal,
                 index_file_path_, response_thread, completion_callback));
}

void SimpleIndexFile::WriteToDisk(const SimpleIndex::EntrySet& entry_set,
                                  uint64 cache_size,
                                  const base::TimeTicks& start,
                                  bool app_on_background) {
  IndexMetadata index_metadata(entry_set.size(), cache_size);
  scoped_ptr<Pickle> pickle = Serialize(index_metadata, entry_set);
  cache_thread_->PostTask(FROM_HERE, base::Bind(
      &WriteToDiskInternal,
      index_file_path_,
      base::Passed(&pickle),
      base::TimeTicks::Now(),
      app_on_background));
}

void SimpleIndexFile::DoomEntrySet(
    scoped_ptr<std::vector<uint64> > entry_hashes,
    const base::Callback<void(int)>& reply_callback) {
  scoped_ptr<int> result(new int());
  int* result_p(result.get());

  worker_pool_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&SimpleSynchronousEntry::DoomEntrySet,
                 base::Passed(entry_hashes.Pass()), index_file_path_.DirName(),
                 result_p),
      base::Bind(&DoomEntrySetReply, base::Passed(result.Pass()),
                 reply_callback));
}

// static
bool SimpleIndexFile::IsIndexFileStale(const base::FilePath& index_filename) {
  base::Time index_mtime;
  base::Time dir_mtime;
  if (!simple_util::GetMTime(index_filename.DirName(), &dir_mtime))
    return true;
  if (!simple_util::GetMTime(index_filename, &index_mtime))
    return true;
  // Index file last_modified must be equal to the directory last_modified since
  // the last operation we do is ReplaceFile in the
  // SimpleIndexFile::WriteToDisk().
  // If not true, we need to restore the index.
  return index_mtime < dir_mtime;
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndexFile::LoadFromDisk(
    const base::FilePath& index_filename) {
  std::string contents;
  if (!file_util::ReadFileToString(index_filename, &contents)) {
    LOG(WARNING) << "Could not read Simple Index file.";
    file_util::Delete(index_filename, false);
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  scoped_ptr<SimpleIndex::EntrySet> entries =
      SimpleIndexFile::Deserialize(contents.data(), contents.size());
  if (!entries) {
    file_util::Delete(index_filename, false);
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  return entries.Pass();
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndexFile::Deserialize(const char* data,
                                                               int data_len) {
  DCHECK(data);
  Pickle pickle(data, data_len);
  if (!pickle.data()) {
    LOG(WARNING) << "Corrupt Simple Index File.";
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  PickleIterator pickle_it(pickle);

  SimpleIndexFile::PickleHeader* header_p =
      pickle.headerT<SimpleIndexFile::PickleHeader>();
  const uint32 crc_read = header_p->crc;
  const uint32 crc_calculated = CalculatePickleCRC(pickle);

  if (crc_read != crc_calculated) {
    LOG(WARNING) << "Invalid CRC in Simple Index file.";
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  SimpleIndexFile::IndexMetadata index_metadata;
  if (!index_metadata.Deserialize(&pickle_it)) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  if (!index_metadata.CheckIndexMetadata()) {
    LOG(ERROR) << "Invalid index_metadata on Simple Cache Index.";
    return scoped_ptr<SimpleIndex::EntrySet>();
  }

  scoped_ptr<SimpleIndex::EntrySet> index_file_entries(
      new SimpleIndex::EntrySet());
  while (index_file_entries->size() < index_metadata.GetNumberOfEntries()) {
    uint64 hash_key;
    EntryMetadata entry_metadata;
    if (!pickle_it.ReadUInt64(&hash_key) ||
        !entry_metadata.Deserialize(&pickle_it)) {
      LOG(WARNING) << "Invalid EntryMetadata in Simple Index file.";
      return scoped_ptr<SimpleIndex::EntrySet>();
    }
    SimpleIndex::InsertInEntrySet(
        hash_key, entry_metadata, index_file_entries.get());
  }

  return index_file_entries.Pass();
}

// static
scoped_ptr<Pickle> SimpleIndexFile::Serialize(
    const SimpleIndexFile::IndexMetadata& index_metadata,
    const SimpleIndex::EntrySet& entries) {
  scoped_ptr<Pickle> pickle(new Pickle(sizeof(SimpleIndexFile::PickleHeader)));

  index_metadata.Serialize(pickle.get());
  for (SimpleIndex::EntrySet::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    pickle->WriteUInt64(it->first);
    it->second.Serialize(pickle.get());
  }
  SimpleIndexFile::PickleHeader* header_p =
      pickle->headerT<SimpleIndexFile::PickleHeader>();
  header_p->crc = CalculatePickleCRC(*pickle);
  return pickle.Pass();
}

// static
void SimpleIndexFile::LoadIndexEntriesInternal(
    const base::FilePath& index_file_path,
    scoped_refptr<base::SingleThreadTaskRunner> response_thread,
    const IndexCompletionCallback& completion_callback) {
  // TODO(felipeg): probably could load a stale index and use it for something.
  scoped_ptr<SimpleIndex::EntrySet> index_file_entries;

  const bool index_file_exists = file_util::PathExists(index_file_path);

  // Only load if the index is not stale.
  const bool index_stale = IsIndexFileStale(index_file_path);
  if (!index_stale) {
    const base::TimeTicks start = base::TimeTicks::Now();
    index_file_entries = LoadFromDisk(index_file_path);
    UMA_HISTOGRAM_TIMES("SimpleCache.IndexLoadTime",
                        base::TimeTicks::Now() - start);
    UMA_HISTOGRAM_COUNTS("SimpleCache.IndexEntriesLoaded",
                         index_file_entries ? index_file_entries->size() : 0);
  }

  UMA_HISTOGRAM_BOOLEAN("SimpleCache.IndexStale", index_stale);

  bool force_index_flush = false;
  if (!index_file_entries) {
    const base::TimeTicks start = base::TimeTicks::Now();
    index_file_entries = RestoreFromDisk(index_file_path);
    UMA_HISTOGRAM_MEDIUM_TIMES("SimpleCache.IndexRestoreTime",
                        base::TimeTicks::Now() - start);
    UMA_HISTOGRAM_COUNTS("SimpleCache.IndexEntriesRestored",
                         index_file_entries->size());

    // When we restore from disk we write the merged index file to disk right
    // away, this might save us from having to restore again next time.
    force_index_flush = true;
  }
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.IndexCorrupt",
                        (!index_stale && force_index_flush));

  // Used in histograms. Please only add new values at the end.
  enum {
    INITIALIZE_METHOD_RECOVERED = 0,
    INITIALIZE_METHOD_LOADED = 1,
    INITIALIZE_METHOD_NEWCACHE = 2,
    INITIALIZE_METHOD_MAX = 3,
  };
  int initialize_method;
  if (index_file_exists) {
    if (force_index_flush)
      initialize_method = INITIALIZE_METHOD_RECOVERED;
    else
      initialize_method = INITIALIZE_METHOD_LOADED;
  } else {
    UMA_HISTOGRAM_COUNTS("SimpleCache.IndexCreatedEntryCount",
                         index_file_entries->size());
    initialize_method = INITIALIZE_METHOD_NEWCACHE;
  }

  UMA_HISTOGRAM_ENUMERATION("SimpleCache.IndexInitializeMethod",
                            initialize_method, INITIALIZE_METHOD_MAX);
  response_thread->PostTask(FROM_HERE,
                            base::Bind(completion_callback,
                                       base::Passed(&index_file_entries),
                                       force_index_flush));
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndexFile::RestoreFromDisk(
    const base::FilePath& index_file_path) {
  LOG(INFO) << "Simple Cache Index is being restored from disk.";

  file_util::Delete(index_file_path, /* recursive = */ false);
  scoped_ptr<SimpleIndex::EntrySet> index_file_entries(
      new SimpleIndex::EntrySet());

  // TODO(felipeg,gavinp): Fix this once we have a one-file per entry format.
  COMPILE_ASSERT(kSimpleEntryFileCount == 3,
                 file_pattern_must_match_file_count);

  const int kFileSuffixLength = sizeof("_0") - 1;
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*_[0-2]");
  base::FileEnumerator enumerator(index_file_path.DirName(),
                                  false /* recursive */,
                                  base::FileEnumerator::FILES,
                                  file_pattern);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    const base::FilePath::StringType base_name = file_path.BaseName().value();
    // Converting to std::string is OK since we never use UTF8 wide chars in our
    // file names.
    const std::string hash_key_string(base_name.begin(),
                                      base_name.end() - kFileSuffixLength);
    uint64 hash_key = 0;
    if (!simple_util::GetEntryHashKeyFromHexString(
            hash_key_string, &hash_key)) {
      LOG(WARNING) << "Invalid Entry Hash Key filename while restoring "
                   << "Simple Index from disk: " << base_name;
      // TODO(felipeg): Should we delete the invalid file here ?
      continue;
    }

    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    base::Time last_used_time;
#if defined(OS_POSIX)
    // For POSIX systems, a last access time is available. However, it's not
    // guaranteed to be more accurate than mtime. It is no worse though.
    last_used_time = base::Time::FromTimeT(info.stat().st_atime);
#endif
    if (last_used_time.is_null())
      last_used_time = info.GetLastModifiedTime();

    int64 file_size = info.GetSize();
    SimpleIndex::EntrySet::iterator it = index_file_entries->find(hash_key);
    if (it == index_file_entries->end()) {
      SimpleIndex::InsertInEntrySet(
          hash_key,
          EntryMetadata(last_used_time, file_size),
          index_file_entries.get());
    } else {
      // Summing up the total size of the entry through all the *_[0-2] files
      it->second.SetEntrySize(it->second.GetEntrySize() + file_size);
    }
  }
  return index_file_entries.Pass();
}

}  // namespace disk_cache
