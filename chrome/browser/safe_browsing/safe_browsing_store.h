// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/hash_tables.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"

class FilePath;

// SafeBrowsingStore provides a storage abstraction for the
// safe-browsing data used to build the bloom filter.  The items
// stored are:
//   The set of add and sub chunks seen.
//   List of SBAddPrefix (chunk_id and SBPrefix).
//   List of SBSubPrefix (chunk_id and the target SBAddPrefix).
//   List of SBAddFullHash (SBAddPrefix, time received and an SBFullHash).
//   List of SBSubFullHash (chunk_id, target SBAddPrefix, and an SBFullHash).
//
// The store is geared towards updating the data, not runtime access
// to the data (that is handled by SafeBrowsingDatabase).  Updates are
// handled similar to a SQL transaction cycle, with the new data being
// returned from FinishUpdate() (the COMMIT).  Data is not persistent
// until FinishUpdate() returns successfully.
//
// FinishUpdate() also handles dropping items who's chunk has been
// deleted, and netting out the add/sub lists (when a sub matches an
// add, both are dropped).

// GetAddChunkId(), GetAddPrefix() and GetFullHash() are exposed so
// that these items can be generically compared with each other by
// SBAddPrefixLess() and SBAddPrefixHashLess().

struct SBAddPrefix {
  int32 chunk_id;
  SBPrefix prefix;

  SBAddPrefix(int32 id, SBPrefix p) : chunk_id(id), prefix(p) {}
  SBAddPrefix() : chunk_id(), prefix() {}

  int32 GetAddChunkId() const { return chunk_id; }
  SBPrefix GetAddPrefix() const { return prefix; }
};

typedef std::deque<SBAddPrefix> SBAddPrefixes;

struct SBSubPrefix {
  int32 chunk_id;
  int32 add_chunk_id;
  SBPrefix add_prefix;

  SBSubPrefix(int32 id, int32 add_id, int prefix)
      : chunk_id(id), add_chunk_id(add_id), add_prefix(prefix) {}
  SBSubPrefix() : chunk_id(), add_chunk_id(), add_prefix() {}

  int32 GetAddChunkId() const { return add_chunk_id; }
  SBPrefix GetAddPrefix() const { return add_prefix; }
};

struct SBAddFullHash {
  int32 chunk_id;
  int32 received;
  SBFullHash full_hash;

  SBAddFullHash(int32 id, base::Time r, const SBFullHash& h)
      : chunk_id(id),
        received(static_cast<int32>(r.ToTimeT())),
        full_hash(h) {
  }

  // Provided for ReadAddHashes() implementations, which already have
  // an int32 for the time.
  SBAddFullHash(int32 id, int32 r, const SBFullHash& h)
      : chunk_id(id), received(r), full_hash(h) {}

  SBAddFullHash() : chunk_id(), received(), full_hash() {}

  int32 GetAddChunkId() const { return chunk_id; }
  SBPrefix GetAddPrefix() const { return full_hash.prefix; }
};

struct SBSubFullHash {
  int32 chunk_id;
  int32 add_chunk_id;
  SBFullHash full_hash;

  SBSubFullHash(int32 id, int32 add_id, const SBFullHash& h)
      : chunk_id(id), add_chunk_id(add_id), full_hash(h) {}
  SBSubFullHash() : chunk_id(), add_chunk_id(), full_hash() {}

  int32 GetAddChunkId() const { return add_chunk_id; }
  SBPrefix GetAddPrefix() const { return full_hash.prefix; }
};

// Determine less-than based on add chunk and prefix.
template <class T, class U>
bool SBAddPrefixLess(const T& a, const U& b) {
  if (a.GetAddChunkId() != b.GetAddChunkId())
    return a.GetAddChunkId() < b.GetAddChunkId();

  return a.GetAddPrefix() < b.GetAddPrefix();
}

// Determine less-than based on add chunk, prefix, and full hash.
// Prefix can compare differently than hash due to byte ordering,
// so it must take precedence.
template <class T, class U>
bool SBAddPrefixHashLess(const T& a, const U& b) {
  if (SBAddPrefixLess(a, b))
    return true;

  if (SBAddPrefixLess(b, a))
    return false;

  return memcmp(a.full_hash.full_hash, b.full_hash.full_hash,
                sizeof(a.full_hash.full_hash)) < 0;
}

// Process the lists for subs which knock out adds.  For any item in
// |sub_prefixes| which has a match in |add_prefixes|, knock out the
// matched items from all vectors.  Additionally remove items from
// deleted chunks.
//
// TODO(shess): Since the prefixes are uniformly-distributed hashes,
// there aren't many ways to organize the inputs for efficient
// processing.  For this reason, the vectors are sorted and processed
// in parallel.  At this time this code does the sorting internally,
// but it might make sense to make sorting an API requirement so that
// the storage can optimize for it.
//
// TODO(shess): The original code did not process |sub_full_hashes|
// for matches in |add_full_hashes|, so this code doesn't, either.  I
// think this is probably a bug.
void SBProcessSubs(SBAddPrefixes* add_prefixes,
                   std::vector<SBSubPrefix>* sub_prefixes,
                   std::vector<SBAddFullHash>* add_full_hashes,
                   std::vector<SBSubFullHash>* sub_full_hashes,
                   const base::hash_set<int32>& add_chunks_deleted,
                   const base::hash_set<int32>& sub_chunks_deleted);

// Records a histogram of the number of items in |prefix_misses| which
// are not in |add_prefixes|.
void SBCheckPrefixMisses(const SBAddPrefixes& add_prefixes,
                         const std::set<SBPrefix>& prefix_misses);

// TODO(shess): This uses int32 rather than int because it's writing
// specifically-sized items to files.  SBPrefix should likewise be
// explicitly sized.

// Abstract interface for storing data.
class SafeBrowsingStore {
 public:
  SafeBrowsingStore() {}
  virtual ~SafeBrowsingStore() {}

  // Sets up the information for later use, but does not necessarily
  // check whether the underlying file exists, or is valid.  If
  // |curruption_callback| is non-NULL it will be called if corruption
  // is detected, which could happen as part of any call other than
  // Delete().  The appropriate action is to use Delete() to clear the
  // store.
  virtual void Init(const FilePath& filename,
                    const base::Closure& corruption_callback) = 0;

  // Deletes the files which back the store, returning true if
  // successful.
  virtual bool Delete() = 0;

  // Get all Add prefixes out from the store.
  virtual bool GetAddPrefixes(SBAddPrefixes* add_prefixes) = 0;

  // Get all add full-length hashes.
  virtual bool GetAddFullHashes(
      std::vector<SBAddFullHash>* add_full_hashes) = 0;

  // Start an update.  None of the following methods should be called
  // unless this returns true.  If this returns true, the update
  // should be terminated by FinishUpdate() or CancelUpdate().
  virtual bool BeginUpdate() = 0;

  // Start a chunk of data.  None of the methods through FinishChunk()
  // should be called unless this returns true.
  // TODO(shess): Would it make sense for this to accept |chunk_id|?
  // Possibly not, because of possible confusion between sub_chunk_id
  // and add_chunk_id.
  virtual bool BeginChunk() = 0;

  virtual bool WriteAddPrefix(int32 chunk_id, SBPrefix prefix) = 0;
  virtual bool WriteAddHash(int32 chunk_id,
                            base::Time receive_time,
                            const SBFullHash& full_hash) = 0;
  virtual bool WriteSubPrefix(int32 chunk_id,
                              int32 add_chunk_id, SBPrefix prefix) = 0;
  virtual bool WriteSubHash(int32 chunk_id, int32 add_chunk_id,
                            const SBFullHash& full_hash) = 0;

  // Collect the chunk data and preferrably store it on disk to
  // release memory.  Shoul not modify the data in-place.
  virtual bool FinishChunk() = 0;

  // Track the chunks which have been seen.
  virtual void SetAddChunk(int32 chunk_id) = 0;
  virtual bool CheckAddChunk(int32 chunk_id) = 0;
  virtual void GetAddChunks(std::vector<int32>* out) = 0;
  virtual void SetSubChunk(int32 chunk_id) = 0;
  virtual bool CheckSubChunk(int32 chunk_id) = 0;
  virtual void GetSubChunks(std::vector<int32>* out) = 0;

  // Delete the indicated chunk_id.  The chunk will continue to be
  // visible until the end of the transaction.
  virtual void DeleteAddChunk(int32 chunk_id) = 0;
  virtual void DeleteSubChunk(int32 chunk_id) = 0;

  // Pass the collected chunks through SBPRocessSubs() and commit to
  // permanent storage.  The resulting add prefixes and hashes will be
  // stored in |add_prefixes_result| and |add_full_hashes_result|.
  // |pending_adds| is the set of full hashes which have been received
  // since the previous update, and is provided as a convenience
  // (could be written via WriteAddHash(), but that would flush the
  // chunk to disk).  |prefix_misses| is the set of prefixes where the
  // |GetHash()| request returned no full hashes, used for diagnostic
  // purposes.
  virtual bool FinishUpdate(
      const std::vector<SBAddFullHash>& pending_adds,
      const std::set<SBPrefix>& prefix_misses,
      SBAddPrefixes* add_prefixes_result,
      std::vector<SBAddFullHash>* add_full_hashes_result) = 0;

  // Cancel the update in process and remove any temporary disk
  // storage, leaving the original data unmodified.
  virtual bool CancelUpdate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingStore);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_H_
