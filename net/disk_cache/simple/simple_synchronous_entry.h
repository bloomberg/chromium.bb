// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/time/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

class SimpleSynchronousEntry;

struct SimpleEntryStat {
  SimpleEntryStat();
  SimpleEntryStat(base::Time last_used_p,
                  base::Time last_modified_p,
                  const int32 data_size_p[]);

  base::Time last_used;
  base::Time last_modified;
  int32 data_size[kSimpleEntryFileCount];
};

struct SimpleEntryCreationResults {
  SimpleEntryCreationResults(SimpleEntryStat entry_stat);
  ~SimpleEntryCreationResults();

  SimpleSynchronousEntry* sync_entry;
  SimpleEntryStat entry_stat;
  int result;
};

// Worker thread interface to the very simple cache. This interface is not
// thread safe, and callers must ensure that it is only ever accessed from
// a single thread between synchronization points.
class SimpleSynchronousEntry {
 public:
  struct CRCRecord {
    CRCRecord();
    CRCRecord(int index_p, bool has_crc32_p, uint32 data_crc32_p);

    int index;
    bool has_crc32;
    uint32 data_crc32;
  };

  struct EntryOperationData {
    EntryOperationData(int index_p, int offset_p, int buf_len_p);
    EntryOperationData(int index_p,
                       int offset_p,
                       int buf_len_p,
                       bool truncate_p);

    int index;
    int offset;
    int buf_len;
    bool truncate;
  };

  static void OpenEntry(const base::FilePath& path,
                        uint64 entry_hash,
                        bool had_index,
                        SimpleEntryCreationResults* out_results);

  static void CreateEntry(const base::FilePath& path,
                          const std::string& key,
                          uint64 entry_hash,
                          bool had_index,
                          SimpleEntryCreationResults* out_results);

  // Deletes an entry without first Opening it. Does not check if there is
  // already an Entry object in memory holding the open files. Be careful! This
  // is meant to be used by the Backend::DoomEntry() call. |callback| will be
  // run by |callback_runner|.
  static void DoomEntry(const base::FilePath& path,
                        const std::string& key,
                        uint64 entry_hash,
                        int* out_result);

  // Like |DoomEntry()| above. Deletes all entries corresponding to the
  // |key_hashes|. Succeeds only when all entries are deleted. Returns a net
  // error code.
  static int DoomEntrySet(scoped_ptr<std::vector<uint64> > key_hashes,
                          const base::FilePath& path);

  // N.B. ReadData(), WriteData(), CheckEOFRecord() and Close() may block on IO.
  void ReadData(const EntryOperationData& in_entry_op,
                net::IOBuffer* out_buf,
                uint32* out_crc32,
                base::Time* out_last_used,
                int* out_result) const;
  void WriteData(const EntryOperationData& in_entry_op,
                 net::IOBuffer* in_buf,
                 SimpleEntryStat* out_entry_stat,
                 int* out_result) const;
  void CheckEOFRecord(int index,
                      int data_size,
                      uint32 expected_crc32,
                      int* out_result) const;

  // Close all streams, and add write EOF records to streams indicated by the
  // CRCRecord entries in |crc32s_to_write|.
  void Close(const SimpleEntryStat& entry_stat,
             scoped_ptr<std::vector<CRCRecord> > crc32s_to_write);

  const base::FilePath& path() const { return path_; }
  std::string key() const { return key_; }

 private:
  SimpleSynchronousEntry(
      const base::FilePath& path,
      const std::string& key,
      uint64 entry_hash);

  // Like Entry, the SimpleSynchronousEntry self releases when Close() is
  // called.
  ~SimpleSynchronousEntry();

  bool OpenOrCreateFiles(bool create,
                         bool had_index,
                         SimpleEntryStat* out_entry_stat);
  void CloseFiles();

  // Returns a net error, i.e. net::OK on success.  |had_index| is passed
  // from the main entry for metrics purposes, and is true if the index was
  // initialized when the open operation began.
  int InitializeForOpen(bool had_index, SimpleEntryStat* out_entry_stat);

  // Returns a net error, including net::OK on success and net::FILE_EXISTS
  // when the entry already exists.  |had_index| is passed from the main entry
  // for metrics purposes, and is true if the index was initialized when the
  // create operation began.
  int InitializeForCreate(bool had_index, SimpleEntryStat* out_entry_stat);

  void Doom() const;

  static bool DeleteFilesForEntryHash(const base::FilePath& path,
                                      uint64 entry_hash);

  const base::FilePath path_;
  const uint64 entry_hash_;
  std::string key_;

  bool have_open_files_;
  bool initialized_;

  base::PlatformFile files_[kSimpleEntryFileCount];
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
