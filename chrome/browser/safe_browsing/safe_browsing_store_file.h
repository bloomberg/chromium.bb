// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_FILE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_FILE_H_

#include <set>
#include <vector>

#include "chrome/browser/safe_browsing/safe_browsing_store.h"

#include "base/file_util.h"

// Implement SafeBrowsingStore in terms of a flat file.  The file
// format is pretty literal:
//
// int32 magic;             // magic number "validating" file
// int32 version;           // format version
//
// // Counts for the various data which follows the header.
// int32 add_chunk_count;   // Chunks seen, including empties.
// int32 sub_chunk_count;   // Ditto.
// int32 add_prefix_count;
// int32 sub_prefix_count;
// int32 add_hash_count;
// int32 sub_hash_count;
//
// array[add_chunk_count] {
//   int32 chunk_id;
// }
// array[sub_chunk_count] {
//   int32 chunk_id;
// }
// array[add_prefix_count] {
//   int32 chunk_id;
//   int32 prefix;
// }
// array[sub_prefix_count] {
//   int32 chunk_id;
//   int32 add_chunk_id;
//   int32 add_prefix;
// }
// array[add_hash_count] {
//   int32 chunk_id;
//   // TODO(shess): This duplicates first four bytes of full_hash!
//   int32 prefix;
//   // From base::Time::ToTimeT().
//   // TODO(shess): an int32 probably has enough resolution.
//   int64 received_time;
//   char[32] full_hash;
// array[sub_hash_count] {
//   int32 chunk_id;
//   int32 add_chunk_id;
//   int32 add_prefix;
//   char[32] add_full_hash;
// }
// TODO(shess): Would a checksum be worthwhile?  If so, check at open,
// or at commit?
//
// During the course of an update, uncommitted data is stored in a
// temporary file (which is later re-used to commit).  This is an
// array of chunks, with the count kept in memory until the end of the
// transaction.  The format of this file is like the main file, with
// the list of chunks seen omitted, as that data is tracked in-memory:
//
// array[] {
//   int32 add_prefix_count;
//   int32 sub_prefix_count;
//   int32 add_hash_count;
//   int32 sub_hash_count;
//   array[add_prefix_count] {
//     int32 chunk_id;
//     int32 prefix;
//   }
//   array[sub_prefix_count] {
//     int32 chunk_id;
//     int32 add_chunk_id;
//     int32 add_prefix;
//   }
//   array[add_hash_count] {
//     int32 chunk_id;
//     int32 prefix;
//     int64 received_time;
//     char[32] full_hash;
//   array[sub_hash_count] {
//     int32 chunk_id;
//     int32 add_chunk_id;
//     int32 add_prefix;
//     char[32] add_full_hash;
//   }
// }
//
// The overall transaction works like this:
// - Open the original file to get the chunks-seen data.
// - Open a temp file for storing new chunk info.
// - Write new chunks to the temp file.
// - When the transaction is finished:
//   - Read the rest of the original file's data into buffers.
//   - Rewind the temp file and merge the new data into buffers.
//   - Process buffers for deletions and apply subs.
//   - Rewind and write the buffers out to temp file.
//   - Delete original file.
//   - Rename temp file to original filename.
//
// TODO(shess): Does there need to be an fsync() before the rename?
// important_file_writer.h seems to think that
// http://valhenson.livejournal.com/37921.html means you don't, but I
// don't think it follows (and, besides, this needs to run on other
// operating systems).
//
// TODO(shess): Using a checksum to validate the file would allow
// correctness without fsync, at the cost of periodically needing to
// regenerate the database from scratch.

// TODO(shess): Regeneration could be moderated by saving the previous
// file, if valid, as a checkpoint.  During update, if the current
// file is found to be invalid, rollback to the checkpoint and run the
// updat forward from there.  This would require that the current file
// be validated at BeginUpdate() rather than FinishUpdate(), because
// the chunks-seen data may have changed.  [Does this have
// implications for the pending_hashes, which were generated while
// using a newer bloom filter?]

class SafeBrowsingStoreFile : public SafeBrowsingStore {
 public:
  SafeBrowsingStoreFile();
  virtual ~SafeBrowsingStoreFile();

  virtual void Init(const FilePath& filename,
                    Callback0::Type* corruption_callback);

  // Delete any on-disk files, including the permanent storage.
  virtual bool Delete();

  virtual bool BeginChunk() {
    return ClearChunkBuffers();
  }
  virtual bool WriteAddPrefix(int32 chunk_id, SBPrefix prefix) {
    add_prefixes_.push_back(SBAddPrefix(chunk_id, prefix));
    return true;
  }
  virtual bool WriteAddHash(int32 chunk_id, SBPrefix prefix,
                            base::Time receive_time, SBFullHash full_hash) {
    add_hashes_.push_back(
        SBAddFullHash(chunk_id, prefix, receive_time, full_hash));
    return true;
  }
  virtual bool WriteSubPrefix(int32 chunk_id,
                              int32 add_chunk_id, SBPrefix prefix) {
    sub_prefixes_.push_back(SBSubPrefix(chunk_id, add_chunk_id, prefix));
    return true;
  }
  virtual bool WriteSubHash(int32 chunk_id, int32 add_chunk_id,
                            SBPrefix prefix, SBFullHash full_hash) {
    sub_hashes_.push_back(
        SBSubFullHash(chunk_id, add_chunk_id, prefix, full_hash));
    return true;
  }
  virtual bool FinishChunk();

  virtual bool BeginUpdate();
  virtual bool DoUpdate(const std::vector<SBAddFullHash>& pending_adds,
                        std::vector<SBAddPrefix>* add_prefixes_result,
                        std::vector<SBAddFullHash>* add_full_hashes_result);
  virtual bool FinishUpdate(const std::vector<SBAddFullHash>& pending_adds,
                            std::vector<SBAddPrefix>* add_prefixes_result,
                            std::vector<SBAddFullHash>* add_full_hashes_result);
  virtual bool CancelUpdate();

  virtual void SetAddChunk(int32 chunk_id) {
    add_chunks_cache_.insert(chunk_id);
  }
  virtual bool CheckAddChunk(int32 chunk_id) {
    return add_chunks_cache_.count(chunk_id) > 0;
  }
  virtual void GetAddChunks(std::vector<int32>* out) {
    out->clear();
    out->insert(out->end(), add_chunks_cache_.begin(), add_chunks_cache_.end());
  }
  virtual void SetSubChunk(int32 chunk_id) {
    sub_chunks_cache_.insert(chunk_id);
  }
  virtual bool CheckSubChunk(int32 chunk_id) {
    return sub_chunks_cache_.count(chunk_id) > 0;
  }
  virtual void GetSubChunks(std::vector<int32>* out) {
    out->clear();
    out->insert(out->end(), sub_chunks_cache_.begin(), sub_chunks_cache_.end());
  }

  virtual void DeleteAddChunk(int32 chunk_id) {
    add_del_cache_.insert(chunk_id);
  }
  virtual void DeleteSubChunk(int32 chunk_id) {
    sub_del_cache_.insert(chunk_id);
  }

  // Returns the name of the temporary file used to buffer data for
  // |filename|.  Exported for unit tests.
  static const FilePath TemporaryFileForFilename(const FilePath& filename) {
    return FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
  }

 private:
  // Close all files and clear all buffers.
  bool Close();

  // Helpers to read/write the various data sets.  Excepting
  // ReadChunksToSet(), which is called too early, the readers skip
  // items from deleted chunks (listed in add_del_cache_ and
  // sub_del_cache_).
  bool ReadChunksToSet(FILE* fp, std::set<int32>* chunks, int count);
  bool WriteChunksFromSet(const std::set<int32>& chunks);
  bool ReadAddPrefixes(FILE* fp,
                       std::vector<SBAddPrefix>* add_prefixes, int count);
  bool WriteAddPrefixes(const std::vector<SBAddPrefix>& add_prefixes);
  bool ReadSubPrefixes(FILE* fp,
                       std::vector<SBSubPrefix>* sub_prefixes, int count);
  bool WriteSubPrefixes(std::vector<SBSubPrefix>& sub_prefixes);
  bool ReadAddHashes(FILE* fp,
                     std::vector<SBAddFullHash>* add_hashes, int count);
  bool WriteAddHashes(const std::vector<SBAddFullHash>& add_hashes);
  bool ReadSubHashes(FILE* fp,
                     std::vector<SBSubFullHash>* sub_hashes, int count);
  bool WriteSubHashes(std::vector<SBSubFullHash>& sub_hashes);

  // Calls |corruption_callback_| if non-NULL, always returns false as
  // a convenience to the caller.
  bool OnCorruptDatabase();

  // Clear temporary buffers used to accumulate chunk data.
  bool ClearChunkBuffers() {
    // NOTE: .clear() doesn't release memory.
    // TODO(shess): Figure out if this is overkill.  Some amount of
    // pre-reserved space is probably reasonable between each chunk
    // collected.
    std::vector<SBAddPrefix>().swap(add_prefixes_);
    std::vector<SBSubPrefix>().swap(sub_prefixes_);
    std::vector<SBAddFullHash>().swap(add_hashes_);
    std::vector<SBSubFullHash>().swap(sub_hashes_);
    return true;
  }

  // Clear all buffers used during update.
  void ClearUpdateBuffers() {
    ClearChunkBuffers();
    chunks_written_ = 0;
    std::set<int32>().swap(add_chunks_cache_);
    std::set<int32>().swap(sub_chunks_cache_);
    base::hash_set<int32>().swap(add_del_cache_);
    base::hash_set<int32>().swap(sub_del_cache_);
  }

  // Buffers for collecting data between BeginChunk() and
  // FinishChunk().
  std::vector<SBAddPrefix> add_prefixes_;
  std::vector<SBSubPrefix> sub_prefixes_;
  std::vector<SBAddFullHash> add_hashes_;
  std::vector<SBSubFullHash> sub_hashes_;

  // Count of chunks collected in |new_file_|.
  int chunks_written_;

  // Name of the main database file.
  FilePath filename_;

  // Handles to the main and scratch files.  |empty_| is true if the
  // main file didn't exist when the update was started.
  file_util::ScopedFILE file_;
  file_util::ScopedFILE new_file_;
  bool empty_;

  // Cache of chunks which have been seen.  Loaded from the database
  // on BeginUpdate() so that it can be queried during the
  // transaction.
  std::set<int32> add_chunks_cache_;
  std::set<int32> sub_chunks_cache_;

  // Cache the set of deleted chunks during a transaction, applied on
  // FinishUpdate().
  // TODO(shess): If the set is small enough, hash_set<> might be
  // slower than plain set<>.
  base::hash_set<int32> add_del_cache_;
  base::hash_set<int32> sub_del_cache_;

  scoped_ptr<Callback0::Type> corruption_callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingStoreFile);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_FILE_H_
