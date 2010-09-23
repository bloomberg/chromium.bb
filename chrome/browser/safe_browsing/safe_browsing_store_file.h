// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_FILE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_FILE_H_
#pragma once

#include <set>
#include <vector>

#include "chrome/browser/safe_browsing/safe_browsing_store.h"

#include "base/callback.h"
#include "base/file_util.h"

// TODO(shess): Data is migrated from SafeBrowsingStoreSqlite as part
// of the next update.  That way everyone doesn't pull a new database
// when the code rolls out (and the migration code isn't gnarly).
// After substantially everyone has migrated, the migration code can
// be removed.  Make sure that it deletes any journal file.

// Implement SafeBrowsingStore in terms of a flat file.  The file
// format is pretty literal:
//
// int32 magic;             // magic number "validating" file
// int32 version;           // format version
//
// // Counts for the various data which follows the header.
// uint32 add_chunk_count;   // Chunks seen, including empties.
// uint32 sub_chunk_count;   // Ditto.
// uint32 add_prefix_count;
// uint32 sub_prefix_count;
// uint32 add_hash_count;
// uint32 sub_hash_count;
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
//   int32 received_time;     // From base::Time::ToTimeT().
//   char[32] full_hash;
// array[sub_hash_count] {
//   int32 chunk_id;
//   int32 add_chunk_id;
//   char[32] add_full_hash;
// }
// MD5Digest checksum;      // Checksum over preceeding data.
//
// During the course of an update, uncommitted data is stored in a
// temporary file (which is later re-used to commit).  This is an
// array of chunks, with the count kept in memory until the end of the
// transaction.  The format of this file is like the main file, with
// the list of chunks seen omitted, as that data is tracked in-memory:
//
// array[] {
//   uint32 add_prefix_count;
//   uint32 sub_prefix_count;
//   uint32 add_hash_count;
//   uint32 sub_hash_count;
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
//     int32 received_time;     // From base::Time::ToTimeT().
//     char[32] full_hash;
//   }
//   array[sub_hash_count] {
//     int32 chunk_id;
//     int32 add_chunk_id;
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

// TODO(shess): By using a checksum, this code can avoid doing an
// fsync(), at the possible cost of more frequently retrieving the
// full dataset.  Measure how often this occurs, and if it occurs too
// often, consider retaining the last known-good file for recovery
// purposes, rather than deleting it.

// TODO(shess): Remove after migration.
class SafeBrowsingStoreSqlite;

class SafeBrowsingStoreFile : public SafeBrowsingStore {
 public:
  SafeBrowsingStoreFile();
  virtual ~SafeBrowsingStoreFile();

  virtual void Init(const FilePath& filename,
                    Callback0::Type* corruption_callback);

  // Delete any on-disk files, including the permanent storage.
  virtual bool Delete();

  virtual bool BeginChunk();
  virtual bool WriteAddPrefix(int32 chunk_id, SBPrefix prefix);
  virtual bool WriteAddHash(int32 chunk_id,
                            base::Time receive_time, SBFullHash full_hash);
  virtual bool WriteSubPrefix(int32 chunk_id,
                              int32 add_chunk_id, SBPrefix prefix);
  virtual bool WriteSubHash(int32 chunk_id, int32 add_chunk_id,
                            SBFullHash full_hash);
  virtual bool FinishChunk();

  virtual bool BeginUpdate();
  virtual bool DoUpdate(const std::vector<SBAddFullHash>& pending_adds,
                        std::vector<SBAddPrefix>* add_prefixes_result,
                        std::vector<SBAddFullHash>* add_full_hashes_result);
  virtual bool FinishUpdate(const std::vector<SBAddFullHash>& pending_adds,
                            std::vector<SBAddPrefix>* add_prefixes_result,
                            std::vector<SBAddFullHash>* add_full_hashes_result);
  virtual bool CancelUpdate();

  virtual void SetAddChunk(int32 chunk_id);
  virtual bool CheckAddChunk(int32 chunk_id);
  virtual void GetAddChunks(std::vector<int32>* out);
  virtual void SetSubChunk(int32 chunk_id);
  virtual bool CheckSubChunk(int32 chunk_id);
  virtual void GetSubChunks(std::vector<int32>* out);

  virtual void DeleteAddChunk(int32 chunk_id);
  virtual void DeleteSubChunk(int32 chunk_id);

  // Returns the name of the temporary file used to buffer data for
  // |filename|.  Exported for unit tests.
  static const FilePath TemporaryFileForFilename(const FilePath& filename) {
    return FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
  }

 private:
  // Close all files and clear all buffers.
  bool Close();

  // Calls |corruption_callback_| if non-NULL, always returns false as
  // a convenience to the caller.
  bool OnCorruptDatabase();

  // Helper for creating a corruption callback for |old_store_|.
  // TODO(shess): Remove after migration.
  void HandleCorruptDatabase() {
    OnCorruptDatabase();
  }

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

  // If the main file existed, but was an SQLite store, this is a
  // handle to it.
  // TODO(shess): Remove this (and all references) after migration.
  scoped_ptr<SafeBrowsingStoreSqlite> old_store_;

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
