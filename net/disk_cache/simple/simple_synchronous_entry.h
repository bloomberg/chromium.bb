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
#include "base/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

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

  static void OpenEntry(
      const base::FilePath& path,
      const std::string& key,
      uint64 entry_hash,
      SimpleSynchronousEntry** out_entry,
      int* out_result);

  static void CreateEntry(
      const base::FilePath& path,
      const std::string& key,
      uint64 entry_hash,
      SimpleSynchronousEntry** out_entry,
      int* out_result);

  // Deletes an entry without first Opening it. Does not check if there is
  // already an Entry object in memory holding the open files. Be careful! This
  // is meant to be used by the Backend::DoomEntry() call. |callback| will be
  // run by |callback_runner|.
  static void DoomEntry(const base::FilePath& path,
                        const std::string& key,
                        uint64 entry_hash,
                        int* out_result);

  // Like |DoomEntry()| above. Deletes all entries corresponding to the
  // |key_hashes|. Succeeds only when all entries are deleted.
  static void DoomEntrySet(scoped_ptr<std::vector<uint64> > key_hashes,
                           const base::FilePath& path,
                           int* out_result);

  // N.B. ReadData(), WriteData(), CheckEOFRecord() and Close() may block on IO.
  void ReadData(int index,
                int offset,
                net::IOBuffer* buf,
                int buf_len,
                uint32* out_crc32,
                int* out_result);
  void WriteData(int index,
                 int offset,
                 net::IOBuffer* buf,
                 int buf_len,
                 bool truncate,
                 int* out_result);
  void CheckEOFRecord(int index,
                      uint32 expected_crc32,
                      int* out_result);

  // Close all streams, and add write EOF records to streams indicated by the
  // CRCRecord entries in |crc32s_to_write|.
  void Close(scoped_ptr<std::vector<CRCRecord> > crc32s_to_write);

  const base::FilePath& path() const { return path_; }
  std::string key() const { return key_; }
  base::Time last_used() const { return last_used_; }
  base::Time last_modified() const { return last_modified_; }
  int32 data_size(int index) const { return data_size_[index]; }

  int64 GetFileSize() const;

 private:
  SimpleSynchronousEntry(
      const base::FilePath& path,
      const std::string& key,
      uint64 entry_hash);

  // Like Entry, the SimpleSynchronousEntry self releases when Close() is
  // called.
  ~SimpleSynchronousEntry();

  bool OpenOrCreateFiles(bool create);
  void CloseFiles();

  // Returns a net::Error, i.e. net::OK on success.
  int InitializeForOpen();

  // Returns a net::Error, including net::OK on success and net::FILE_EXISTS
  // when the entry already exists.
  int InitializeForCreate();

  void Doom();

  static bool DeleteFilesForEntryHash(const base::FilePath& path,
                                      uint64 entry_hash);

  const base::FilePath path_;
  const std::string key_;
  const uint64 entry_hash_;

  bool have_open_files_;
  bool initialized_;

  base::Time last_used_;
  base::Time last_modified_;
  int32 data_size_[kSimpleEntryFileCount];

  base::PlatformFile files_[kSimpleEntryFileCount];
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
