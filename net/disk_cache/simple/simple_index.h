// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_disk_format.h"

namespace base {
class TaskRunner;
}

namespace disk_cache {

// This class is not Thread-safe.
class SimpleIndex
    : public base::SupportsWeakPtr<SimpleIndex> {
 public:
  SimpleIndex(
      const scoped_refptr<base::TaskRunner>& cache_thread,
      const base::FilePath& path);

  virtual ~SimpleIndex();

  // Should be called on CacheThread.
  bool Initialize();

  void Insert(const std::string& key);
  void Remove(const std::string& key);

  bool Has(const std::string& key) const;

  // Update the last used time of the entry with the given key and return true
  // iff the entry exist in the index.
  bool UseIfExists(const std::string& key);

  void Cleanup();

  // Update the size (in bytes) of an entry, in the metadata stored in the
  // index. This should be the total disk-file size including all streams of the
  // entry.
  bool UpdateEntrySize(const std::string& key, uint64 entry_size);

 private:
  // TODO(felipeg): This way we are storing the hash_key string twice (as the
  // hash_map::key and as a member of EntryMetadata. We could save space if we
  // redefine the hash_map::operators and make the hash_map::key be part of the
  // EntryMetadata itself.
  typedef base::hash_map<std::string, SimpleIndexFile::EntryMetadata> EntrySet;

  void InsertInternal(const SimpleIndexFile::EntryMetadata& entry_metadata);

  // Enumerates all entries' files on disk and regenerates the index.
  bool RestoreFromDisk();

  // |out_buffer| needs to be pre-allocated. The serialized index is stored in
  // |out_buffer|.
  void Serialize(std::string* out_buffer);

  bool OpenIndexFile();
  bool CloseIndexFile();

  static void UpdateFile(const base::FilePath& index_filename,
                         const base::FilePath& temp_filename,
                         scoped_ptr<std::string> buffer);

  const base::FilePath path_;

  EntrySet entries_set_;
  uint64 cache_size_;  // Total cache storage size in bytes.

  base::FilePath index_filename_;
  base::PlatformFile index_file_;

  // We keep the thread from where Initialize() method has been called so that
  // we run the Cleanup method in the same thread. Usually that should be the
  // CacheThread.
  scoped_refptr<base::TaskRunner> cache_thread_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
