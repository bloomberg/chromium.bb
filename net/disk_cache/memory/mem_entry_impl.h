// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_
#define NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "net/log/net_log.h"

namespace disk_cache {

class MemBackendImpl;

// This class implements the Entry interface for the memory-only cache. An
// object of this class represents a single entry on the cache. We use two
// types of entries, parent and child to support sparse caching.
//
// A parent entry is non-sparse until a sparse method is invoked (i.e.
// ReadSparseData, WriteSparseData, GetAvailableRange) when sparse information
// is initialized. It then manages a list of child entries and delegates the
// sparse API calls to the child entries. It creates and deletes child entries
// and updates the list when needed.
//
// A child entry is used to carry partial cache content, non-sparse methods like
// ReadData and WriteData cannot be applied to them. The lifetime of a child
// entry is managed by the parent entry that created it except that the entry
// can be evicted independently. A child entry does not have a key and it is not
// registered in the backend's entry map. It is registered in the backend's
// ranking list to enable eviction of a partial content.
//
// A sparse entry has a fixed maximum size and can be partially filled. There
// can only be one continous filled region in a sparse entry, as illustrated by
// the following example:
// | xxx ooooo |
// x = unfilled region
// o = filled region
// It is guranteed that there is at most one unfilled region and one filled
// region, and the unfilled region (if there is one) is always before the filled
// region. The book keeping for filled region in a sparse entry is done by using
// the variable |child_first_pos_| (inclusive).

class MemEntryImpl : public Entry {
 public:
  enum EntryType {
    kParentEntry,
    kChildEntry,
  };

  explicit MemEntryImpl(MemBackendImpl* backend);

  // Performs the initialization of a EntryImpl that will be added to the
  // cache.
  bool CreateEntry(const std::string& key, net::NetLog* net_log);

  // Permanently destroys this entry.
  void InternalDoom();

  void Open();
  bool InUse();

  MemEntryImpl* next() const {
    return next_;
  }

  MemEntryImpl* prev() const {
    return prev_;
  }

  void set_next(MemEntryImpl* next) {
    next_ = next;
  }

  void set_prev(MemEntryImpl* prev) {
    prev_ = prev;
  }

  EntryType type() const {
    return parent_ ? kChildEntry : kParentEntry;
  }

  const net::BoundNetLog& net_log() {
    return net_log_;
  }

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
  void CancelSparseIO() override {}
  int ReadyForSparseIO(const CompletionCallback& callback) override;

 private:
  typedef base::hash_map<int, MemEntryImpl*> EntryMap;

  enum {
    NUM_STREAMS = 3
  };

  ~MemEntryImpl() override;

  // Do all the work for corresponding public functions.  Implemented as
  // separate functions to make logging of results simpler.
  int InternalReadData(int index, int offset, IOBuffer* buf, int buf_len);
  int InternalWriteData(int index, int offset, IOBuffer* buf, int buf_len,
                        bool truncate);
  int InternalReadSparseData(int64_t offset, IOBuffer* buf, int buf_len);
  int InternalWriteSparseData(int64_t offset, IOBuffer* buf, int buf_len);

  // Old Entry interface.
  int GetAvailableRange(int64_t offset, int len, int64_t* start);

  // Grows and cleans up the data buffer.
  void PrepareTarget(int index, int offset, int buf_len);

  // Updates ranking information.
  void UpdateRank(bool modified);

  // Initializes the children map and sparse info. This method is only called
  // on a parent entry.
  bool InitSparseInfo();

  // Performs the initialization of a MemEntryImpl as a child entry.
  // |parent| is the pointer to the parent entry. |child_id| is the ID of
  // the new child.
  bool InitChildEntry(MemEntryImpl* parent, int child_id, net::NetLog* net_log);

  // Returns an entry responsible for |offset|. The returned entry can be a
  // child entry or this entry itself if |offset| points to the first range.
  // If such entry does not exist and |create| is true, a new child entry is
  // created.
  MemEntryImpl* OpenChild(int64_t offset, bool create);

  // Finds the first child located within the range [|offset|, |offset + len|).
  // Returns the number of bytes ahead of |offset| to reach the first available
  // bytes in the entry. The first child found is output to |child|.
  int FindNextChild(int64_t offset, int len, MemEntryImpl** child);

  // Removes child indexed by |child_id| from the children map.
  void DetachChild(int child_id);

  std::string key_;
  std::vector<char> data_[NUM_STREAMS];  // User data.
  int32_t data_size_[NUM_STREAMS];
  int ref_count_;

  int child_id_;              // The ID of a child entry.
  int child_first_pos_;       // The position of the first byte in a child
                              // entry.
  MemEntryImpl* next_;        // Pointers for the LRU list.
  MemEntryImpl* prev_;
  MemEntryImpl* parent_;      // Pointer to the parent entry.
  scoped_ptr<EntryMap> children_;

  base::Time last_modified_;  // LRU information.
  base::Time last_used_;
  MemBackendImpl* backend_;   // Back pointer to the cache.
  bool doomed_;               // True if this entry was removed from the cache.

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(MemEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_
