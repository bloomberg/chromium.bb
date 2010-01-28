// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

namespace {

// NOTE(shess): kFileMagic should not be a byte-wise palindrome, so
// that byte-order changes force corruption.
const int32 kFileMagic = 0x600D71FE;
const int32 kFileVersion = 7;  // SQLite storage was 6...
const size_t kFileHeaderSize = 8 * sizeof(int32);

bool ReadInt32(FILE* fp, int32* value) {
  DCHECK(value);
  const size_t ret = fread(value, sizeof(*value), 1, fp);
  return ret == 1;
}

bool WriteInt32(FILE* fp, int32 value) {
  const size_t ret = fwrite(&value, sizeof(value), 1, fp);
  return ret == 1;
}

bool ReadTime(FILE* fp, base::Time* value) {
  DCHECK(value);

  int64 time_t;
  const size_t ret = fread(&time_t, sizeof(time_t), 1, fp);
  if (ret != 1)
    return false;
  *value = base::Time::FromTimeT(time_t);
  return true;
}

bool WriteTime(FILE* fp, const base::Time& value) {
  const int64 time_t = value.ToTimeT();
  const size_t ret = fwrite(&time_t, sizeof(time_t), 1, fp);
  return ret == 1;
}

bool ReadHash(FILE* fp, SBFullHash* value) {
  DCHECK(value);
  const size_t ret = fread(&value->full_hash, sizeof(value->full_hash),
                           1, fp);
  return ret == 1;
}

bool WriteHash(FILE* fp, SBFullHash value) {
  const size_t ret = fwrite(&value.full_hash, sizeof(value.full_hash),
                            1, fp);
  return ret == 1;
}

bool FileSeek(FILE* fp, size_t offset) {
  int rv = fseek(fp, offset, SEEK_SET);
  DCHECK_EQ(rv, 0);
  return rv == 0;
}

// Delete the chunks in |deleted| from |chunks|.
void DeleteChunksFromSet(const base::hash_set<int32>& deleted,
                         std::set<int32>* chunks) {
  for (std::set<int32>::iterator iter = chunks->begin();
       iter != chunks->end();) {
    std::set<int32>::iterator prev = iter++;
    if (deleted.count(*prev) > 0)
      chunks->erase(prev);
  }
}

}  // namespace

SafeBrowsingStoreFile::SafeBrowsingStoreFile()
    : chunks_written_(0),
      file_(NULL) {
}
SafeBrowsingStoreFile::~SafeBrowsingStoreFile() {
  Close();
}

bool SafeBrowsingStoreFile::Delete() {
  // The database should not be open at this point.  But, just in
  // case, close everything before deleting.
  if (!Close()) {
    NOTREACHED();
    return false;
  }

  if (!file_util::Delete(filename_, false) &&
      file_util::PathExists(filename_)) {
    NOTREACHED();
    return false;
  }

  const FilePath new_filename = TemporaryFileForFilename(filename_);
  if (!file_util::Delete(new_filename, false) &&
      file_util::PathExists(new_filename)) {
    NOTREACHED();
    return false;
  }

  return true;
}

void SafeBrowsingStoreFile::Init(const FilePath& filename,
                                 Callback0::Type* corruption_callback) {
  filename_ = filename;
  corruption_callback_.reset(corruption_callback);
}

bool SafeBrowsingStoreFile::OnCorruptDatabase() {
  if (corruption_callback_.get())
    corruption_callback_->Run();

  // Return false as a convenience to callers.
  return false;
}

bool SafeBrowsingStoreFile::Close() {
  ClearUpdateBuffers();

  // Make sure the files are closed.
  file_.reset();
  new_file_.reset();
  return true;
}

bool SafeBrowsingStoreFile::ReadChunksToSet(FILE* fp, std::set<int32>* chunks,
                                            int count) {
  DCHECK(fp);

  for (int i = 0; i < count; ++i) {
    int32 chunk_id;
    if (!ReadInt32(fp, &chunk_id))
      return false;
    chunks->insert(chunk_id);
  }
  return true;
}

bool SafeBrowsingStoreFile::WriteChunksFromSet(const std::set<int32>& chunks) {
  DCHECK(new_file_.get());

  for (std::set<int32>::const_iterator iter = chunks.begin();
       iter != chunks.end(); ++iter) {
    if (!WriteInt32(new_file_.get(), *iter))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreFile::ReadAddPrefixes(
    FILE* fp, std::vector<SBAddPrefix>* add_prefixes, int count) {
  DCHECK(fp && add_prefixes);

  add_prefixes->reserve(add_prefixes->size() + count);

  for (int32 i = 0; i < count; ++i) {
    int32 chunk_id;
    SBPrefix prefix;
    DCHECK_EQ(sizeof(int32), sizeof(prefix));

    if (!ReadInt32(fp, &chunk_id) || !ReadInt32(fp, &prefix))
      return false;

    if (add_del_cache_.count(chunk_id) > 0)
      continue;

    add_prefixes->push_back(SBAddPrefix(chunk_id, prefix));
  }

  return true;
}

bool SafeBrowsingStoreFile::WriteAddPrefixes(
    const std::vector<SBAddPrefix>& add_prefixes) {
  DCHECK(new_file_.get());

  for (std::vector<SBAddPrefix>::const_iterator iter = add_prefixes.begin();
       iter != add_prefixes.end(); ++iter) {
    DCHECK_EQ(sizeof(int32), sizeof(iter->prefix));
    if (!WriteInt32(new_file_.get(), iter->chunk_id) ||
        !WriteInt32(new_file_.get(), iter->prefix))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreFile::ReadSubPrefixes(
    FILE* fp, std::vector<SBSubPrefix>* sub_prefixes, int count) {
  DCHECK(fp && sub_prefixes);

  sub_prefixes->reserve(sub_prefixes->size() + count);

  for (int32 i = 0; i < count; ++i) {
    int32 chunk_id, add_chunk_id;
    SBPrefix add_prefix;
    DCHECK_EQ(sizeof(int32), sizeof(add_prefix));

    if (!ReadInt32(fp, &chunk_id) ||
        !ReadInt32(fp, &add_chunk_id) || !ReadInt32(fp, &add_prefix))
      return false;

    if (sub_del_cache_.count(chunk_id) > 0)
      continue;

    sub_prefixes->push_back(SBSubPrefix(chunk_id, add_chunk_id, add_prefix));
  }

  return true;
}

bool SafeBrowsingStoreFile::WriteSubPrefixes(
    std::vector<SBSubPrefix>& sub_prefixes) {
  DCHECK(new_file_.get());

  for (std::vector<SBSubPrefix>::const_iterator iter = sub_prefixes.begin();
       iter != sub_prefixes.end(); ++iter) {
    if (!WriteInt32(new_file_.get(), iter->chunk_id) ||
        !WriteInt32(new_file_.get(), iter->add_prefix.chunk_id) ||
        !WriteInt32(new_file_.get(), iter->add_prefix.prefix))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreFile::ReadAddHashes(
    FILE* fp, std::vector<SBAddFullHash>* add_hashes, int count) {
  DCHECK(fp && add_hashes);

  add_hashes->reserve(add_hashes->size() + count);

  for (int i = 0; i < count; ++i) {
    int32 chunk_id;
    SBPrefix prefix;
    base::Time received;
    SBFullHash full_hash;
    DCHECK_EQ(sizeof(int32), sizeof(prefix));

    if (!ReadInt32(fp, &chunk_id) ||
        !ReadInt32(fp, &prefix) ||
        !ReadTime(fp, &received) ||
        !ReadHash(fp, &full_hash))
      return false;

    if (add_del_cache_.count(chunk_id) > 0)
      continue;

    add_hashes->push_back(SBAddFullHash(chunk_id, prefix, received, full_hash));
  }

  return true;
}

bool SafeBrowsingStoreFile::WriteAddHashes(
    const std::vector<SBAddFullHash>& add_hashes) {
  DCHECK(new_file_.get());

  for (std::vector<SBAddFullHash>::const_iterator iter = add_hashes.begin();
       iter != add_hashes.end(); ++iter) {
    if (!WriteInt32(new_file_.get(), iter->add_prefix.chunk_id) ||
        !WriteInt32(new_file_.get(), iter->add_prefix.prefix) ||
        !WriteTime(new_file_.get(), iter->received) ||
        !WriteHash(new_file_.get(), iter->full_hash))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreFile::ReadSubHashes(
    FILE* fp, std::vector<SBSubFullHash>* sub_hashes, int count) {
  DCHECK(fp);

  sub_hashes->reserve(sub_hashes->size() + count);

  for (int i = 0; i < count; ++i) {
    int32 chunk_id;
    int32 add_chunk_id;
    SBPrefix add_prefix;
    SBFullHash add_full_hash;
    DCHECK_EQ(sizeof(int32), sizeof(add_prefix));

    if (!ReadInt32(fp, &chunk_id) ||
        !ReadInt32(fp, &add_chunk_id) ||
        !ReadInt32(fp, &add_prefix) ||
        !ReadHash(fp, &add_full_hash))
      return false;

    if (sub_del_cache_.count(chunk_id) > 0)
      continue;

    sub_hashes->push_back(
        SBSubFullHash(chunk_id, add_chunk_id, add_prefix, add_full_hash));
  }

  return true;
}

bool SafeBrowsingStoreFile::WriteSubHashes(
    std::vector<SBSubFullHash>& sub_hashes) {
  DCHECK(new_file_.get());

  for (std::vector<SBSubFullHash>::const_iterator iter = sub_hashes.begin();
       iter != sub_hashes.end(); ++iter) {
    if (!WriteInt32(new_file_.get(), iter->chunk_id) ||
        !WriteInt32(new_file_.get(), iter->add_prefix.chunk_id) ||
        !WriteInt32(new_file_.get(), iter->add_prefix.prefix) ||
        !WriteHash(new_file_.get(), iter->full_hash))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreFile::BeginUpdate() {
  DCHECK(!file_.get() && !new_file_.get());

  // Structures should all be clear unless something bad happened.
  DCHECK(add_chunks_cache_.empty());
  DCHECK(sub_chunks_cache_.empty());
  DCHECK(add_del_cache_.empty());
  DCHECK(sub_del_cache_.empty());
  DCHECK(add_prefixes_.empty());
  DCHECK(sub_prefixes_.empty());
  DCHECK(add_hashes_.empty());
  DCHECK(sub_hashes_.empty());
  DCHECK_EQ(chunks_written_, 0);

  const FilePath new_filename = TemporaryFileForFilename(filename_);
  file_util::ScopedFILE new_file(file_util::OpenFile(new_filename, "wb+"));
  if (new_file.get() == NULL)
    return false;

  file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb"));
  empty_ = (file.get() == NULL);
  if (empty_) {
    // If the file exists but cannot be opened, try to delete it (not
    // deleting directly, the bloom filter needs to be deleted, too).
    if (file_util::PathExists(filename_))
      return OnCorruptDatabase();

    new_file_.swap(new_file);
    return true;
  }

  int32 magic, version;
  if (!ReadInt32(file.get(), &magic) || !ReadInt32(file.get(), &version))
    return OnCorruptDatabase();

  if (magic != kFileMagic || version != kFileVersion)
    return OnCorruptDatabase();

  int32 add_chunk_count, sub_chunk_count;
  if (!ReadInt32(file.get(), &add_chunk_count) ||
      !ReadInt32(file.get(), &sub_chunk_count))
    return OnCorruptDatabase();

  if (!FileSeek(file.get(), kFileHeaderSize))
    return OnCorruptDatabase();

  if (!ReadChunksToSet(file.get(), &add_chunks_cache_, add_chunk_count) ||
      !ReadChunksToSet(file.get(), &sub_chunks_cache_, sub_chunk_count))
    return OnCorruptDatabase();

  file_.swap(file);
  new_file_.swap(new_file);
  return true;
}

bool SafeBrowsingStoreFile::FinishChunk() {
  if (!add_prefixes_.size() && !sub_prefixes_.size() &&
      !add_hashes_.size() && !sub_hashes_.size())
    return true;

  if (!WriteInt32(new_file_.get(), add_prefixes_.size()) ||
      !WriteInt32(new_file_.get(), sub_prefixes_.size()) ||
      !WriteInt32(new_file_.get(), add_hashes_.size()) ||
      !WriteInt32(new_file_.get(), sub_hashes_.size()))
    return false;

  if (!WriteAddPrefixes(add_prefixes_) ||
      !WriteSubPrefixes(sub_prefixes_) ||
      !WriteAddHashes(add_hashes_) ||
      !WriteSubHashes(sub_hashes_))
    return false;

  ++chunks_written_;

  // Clear everything to save memory.
  return ClearChunkBuffers();
}

bool SafeBrowsingStoreFile::DoUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  DCHECK(file_.get() || empty_);
  DCHECK(new_file_.get());

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBAddFullHash> add_full_hashes;
  std::vector<SBSubFullHash> sub_full_hashes;

  // Read |file_| into the vectors.
  if (!empty_) {
    DCHECK(file_.get());

    int32 magic, version;
    int32 add_chunk_count, sub_chunk_count;
    int32 add_prefix_count, sub_prefix_count;
    int32 add_hash_count, sub_hash_count;

    if (!FileSeek(file_.get(), 0))
      return OnCorruptDatabase();

    if (!ReadInt32(file_.get(), &magic) ||
        !ReadInt32(file_.get(), &version) ||
        !ReadInt32(file_.get(), &add_chunk_count) ||
        !ReadInt32(file_.get(), &sub_chunk_count) ||
        !ReadInt32(file_.get(), &add_prefix_count) ||
        !ReadInt32(file_.get(), &sub_prefix_count) ||
        !ReadInt32(file_.get(), &add_hash_count) ||
        !ReadInt32(file_.get(), &sub_hash_count))
      return OnCorruptDatabase();

    if (magic != kFileMagic || version != kFileVersion)
      return OnCorruptDatabase();

    const size_t prefixes_offset = kFileHeaderSize +
        (add_chunk_count + sub_chunk_count) * sizeof(int32);
    if (!FileSeek(file_.get(), prefixes_offset))
      return OnCorruptDatabase();

    if (!ReadAddPrefixes(file_.get(), &add_prefixes, add_prefix_count) ||
        !ReadSubPrefixes(file_.get(), &sub_prefixes, sub_prefix_count) ||
        !ReadAddHashes(file_.get(), &add_full_hashes, add_hash_count) ||
        !ReadSubHashes(file_.get(), &sub_full_hashes, sub_hash_count))
      return OnCorruptDatabase();

    // Close the file so we can later rename over it.
    file_.reset();
  }
  DCHECK(!file_.get());

  // Rewind the temporary storage.
  if (!FileSeek(new_file_.get(), 0))
    return false;

  // Append the accumulated chunks onto the vectors from file_.
  for (int i = 0; i < chunks_written_; ++i) {
    int32 add_prefix_count, sub_prefix_count;
    int32 add_hash_count, sub_hash_count;

    if (!ReadInt32(new_file_.get(), &add_prefix_count) ||
        !ReadInt32(new_file_.get(), &sub_prefix_count) ||
        !ReadInt32(new_file_.get(), &add_hash_count) ||
        !ReadInt32(new_file_.get(), &sub_hash_count))
      return false;

    // TODO(shess): If the vectors were kept sorted, then this code
    // could use std::inplace_merge() to merge everything together in
    // sorted order.  That might still be slower than just sorting at
    // the end if there were a large number of chunks.  In that case
    // some sort of recursive binary merge might be in order (merge
    // chunks pairwise, merge those chunks pairwise, and so on, then
    // merge the result with the main list).
    if (!ReadAddPrefixes(new_file_.get(), &add_prefixes, add_prefix_count) ||
        !ReadSubPrefixes(new_file_.get(), &sub_prefixes, sub_prefix_count) ||
        !ReadAddHashes(new_file_.get(), &add_full_hashes, add_hash_count) ||
        !ReadSubHashes(new_file_.get(), &sub_full_hashes, sub_hash_count))
      return false;
  }

  // Add the pending adds which haven't since been deleted.
  for (std::vector<SBAddFullHash>::const_iterator iter = pending_adds.begin();
       iter != pending_adds.end(); ++iter) {
    if (add_del_cache_.count(iter->add_prefix.chunk_id) == 0)
      add_full_hashes.push_back(*iter);
  }

  // Knock the subs from the adds.
  SBProcessSubs(&add_prefixes, &sub_prefixes,
                &add_full_hashes, &sub_full_hashes);

  // We no longer need to track deleted chunks.
  DeleteChunksFromSet(add_del_cache_, &add_chunks_cache_);
  DeleteChunksFromSet(sub_del_cache_, &sub_chunks_cache_);

  // Write the new data to new_file_.
  // TODO(shess): If we receive a lot of subs relative to adds,
  // overwriting the temporary chunk data in new_file_ with the
  // permanent data could leave additional data at the end.  Won't
  // cause any problems, but does waste space.  There is no truncate()
  // for stdio.  Could use ftruncate() or re-open the file.  Or maybe
  // ignore it, since we'll likely rewrite soon enough.
  if (!FileSeek(new_file_.get(), 0))
    return false;

  if (!WriteInt32(new_file_.get(), kFileMagic) ||
      !WriteInt32(new_file_.get(), kFileVersion) ||
      !WriteInt32(new_file_.get(), add_chunks_cache_.size()) ||
      !WriteInt32(new_file_.get(), sub_chunks_cache_.size()) ||
      !WriteInt32(new_file_.get(), add_prefixes.size()) ||
      !WriteInt32(new_file_.get(), sub_prefixes.size()) ||
      !WriteInt32(new_file_.get(), add_full_hashes.size()) ||
      !WriteInt32(new_file_.get(), sub_full_hashes.size()))
    return false;

  if (!WriteChunksFromSet(add_chunks_cache_) ||
      !WriteChunksFromSet(sub_chunks_cache_) ||
      !WriteAddPrefixes(add_prefixes) ||
      !WriteSubPrefixes(sub_prefixes) ||
      !WriteAddHashes(add_full_hashes) ||
      !WriteSubHashes(sub_full_hashes))
    return false;

  // Close the file handle and swizzle the file into place.
  new_file_.reset();
  if (!file_util::Delete(filename_, false) &&
      file_util::PathExists(filename_))
    return false;

  const FilePath new_filename = TemporaryFileForFilename(filename_);
  if (!file_util::Move(new_filename, filename_)) {
    return false;
  }

  // Pass the resulting data off to the caller.
  add_prefixes_result->swap(add_prefixes);
  add_full_hashes_result->swap(add_full_hashes);

  return true;
}

bool SafeBrowsingStoreFile::FinishUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  bool ret = DoUpdate(pending_adds,
                      add_prefixes_result, add_full_hashes_result);

  if (!ret) {
    CancelUpdate();
    return false;
  }

  DCHECK(!new_file_.get());
  DCHECK(!file_.get());

  return Close();
}

bool SafeBrowsingStoreFile::CancelUpdate() {
  return Close();
}
