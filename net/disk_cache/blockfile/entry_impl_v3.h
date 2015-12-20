// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_ENTRY_IMPL_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_ENTRY_IMPL_V3_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/blockfile/disk_format_v3.h"
#include "net/disk_cache/blockfile/storage_block.h"
#include "net/disk_cache/disk_cache.h"
#include "net/log/net_log.h"

namespace disk_cache {

class BackendImplV3;
class SparseControlV3;

// This class implements the Entry interface. An object of this
// class represents a single entry on the cache.
class NET_EXPORT_PRIVATE EntryImplV3
    : public Entry,
      public base::RefCounted<EntryImplV3> {
  friend class base::RefCounted<EntryImplV3>;
  // friend class SparseControlV3;
 public:
  enum Operation {
    kRead,
    kWrite,
    kSparseRead,
    kSparseWrite,
    kAsyncIO,
    kReadAsync1,
    kWriteAsync1
  };

  EntryImplV3(BackendImplV3* backend, Addr address, bool read_only);

  // Performs the initialization of a EntryImplV3 that will be added to the
  // cache.
  bool CreateEntry(Addr node_address, const std::string& key, uint32_t hash);

  uint32_t GetHash();

  uint32_t GetHash() const;
  Addr GetAddress() const;
  int GetReuseCounter() const;
  void SetReuseCounter(int count);
  int GetRefetchCounter() const;
  void SetRefetchCounter(int count);

  // Returns true if this entry matches the lookup arguments.
  bool IsSameEntry(const std::string& key, uint32_t hash);

  // Permamently destroys this entry.
  void InternalDoom();

  // Returns false if the entry is clearly invalid.
  bool SanityCheck();
  bool DataSanityCheck();

  // Attempts to make this entry reachable though the key.
  void FixForDelete();

  // Set the access times for this entry. This method provides support for
  // the upgrade tool.
  void SetTimes(base::Time last_used, base::Time last_modified);

  // Logs a begin event and enables logging for the EntryImplV3. Will also cause
  // an end event to be logged on destruction. The EntryImplV3 must have its key
  // initialized before this is called. |created| is true if the Entry was
  // created rather than opened.
  void BeginLogging(net::NetLog* net_log, bool created);

  const net::BoundNetLog& net_log() const;

  // Entry interface.
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  base::Time GetLastModified() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               const CompletionCallback& callback) override;
  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                const CompletionCallback& callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) override;
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) override;
  int GetAvailableRange(int64_t offset,
                        int len,
                        int64_t* start,
                        const CompletionCallback& callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override;
  int ReadyForSparseIO(const CompletionCallback& callback) override;

 private:
  enum {
     kNumStreams = 3
  };
  class UserBuffer;

  ~EntryImplV3() override;

  // Do all the work for ReadDataImpl and WriteDataImpl.  Implemented as
  // separate functions to make logging of results simpler.
  int InternalReadData(int index, int offset, IOBuffer* buf,
                       int buf_len, const CompletionCallback& callback);
  int InternalWriteData(int index, int offset, IOBuffer* buf, int buf_len,
                        const CompletionCallback& callback, bool truncate);

  // Initializes the storage for an internal or external data block.
  bool CreateDataBlock(int index, int size);

  // Initializes the storage for an internal or external generic block.
  bool CreateBlock(int size, Addr* address);

  // Deletes the data pointed by address, maybe backed by files_[index].
  // Note that most likely the caller should delete (and store) the reference to
  // |address| *before* calling this method because we don't want to have an
  // entry using an address that is already free.
  void DeleteData(Addr address, int index);

  // Updates ranking information.
  void UpdateRank(bool modified);

  // Deletes this entry from disk. If |everything| is false, only the user data
  // will be removed, leaving the key and control data intact.
  void DeleteEntryData(bool everything);

  // Prepares the target file or buffer for a write of buf_len bytes at the
  // given offset.
  bool PrepareTarget(int index, int offset, int buf_len, bool truncate);

  // Adjusts the internal buffer and file handle for a write that truncates this
  // stream.
  bool HandleTruncation(int index, int offset, int buf_len);

  // Copies data from disk to the internal buffer.
  bool CopyToLocalBuffer(int index);

  // Reads from a block data file to this object's memory buffer.
  bool MoveToLocalBuffer(int index);

  // Loads the external file to this object's memory buffer.
  bool ImportSeparateFile(int index, int new_size);

  // Makes sure that the internal buffer can handle the a write of |buf_len|
  // bytes to |offset|.
  bool PrepareBuffer(int index, int offset, int buf_len);

  // Flushes the in-memory data to the backing storage. The data destination
  // is determined based on the current data length and |min_len|.
  bool Flush(int index, int min_len);

  // Updates the size of a given data stream.
  void UpdateSize(int index, int old_size, int new_size);

  // Initializes the sparse control object. Returns a net error code.
  int InitSparseData();

  // Adds the provided |flags| to the current EntryFlags for this entry.
  void SetEntryFlags(uint32_t flags);

  // Returns the current EntryFlags for this entry.
  uint32_t GetEntryFlags();

  // Gets the data stored at the given index. If the information is in memory,
  // a buffer will be allocated and the data will be copied to it (the caller
  // can find out the size of the buffer before making this call). Otherwise,
  // the cache address of the data will be returned, and that address will be
  // removed from the regular book keeping of this entry so the caller is
  // responsible for deleting the block (or file) from the backing store at some
  // point; there is no need to report any storage-size change, only to do the
  // actual cleanup.
  void GetData(int index, char** buffer, Addr* address);

  // Generates a histogram for the time spent working on this operation.
  void ReportIOTime(Operation op, const base::TimeTicks& start);

  // Logs this entry to the internal trace buffer.
  void Log(const char* msg);

  scoped_ptr<EntryRecord> entry_;  // Basic record for this entry.
  scoped_ptr<ShortEntryRecord> short_entry_;  // Valid for evicted entries.
  base::WeakPtr<BackendImplV3> backend_;  // Back pointer to the cache.
  scoped_ptr<UserBuffer> user_buffers_[kNumStreams];  // Stores user data.
  mutable std::string key_;           // Copy of the key.
  Addr address_;
  int unreported_size_[kNumStreams];  // Bytes not reported yet to the backend.
  bool doomed_;               // True if this entry was removed from the cache.
  bool read_only_;
  bool dirty_;                // True if there is something to write.
  bool modified_;
  // scoped_ptr<SparseControlV3> sparse_;  // Support for sparse entries.

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(EntryImplV3);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_ENTRY_IMPL_V3_H_
