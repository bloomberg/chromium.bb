// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/md5.h"

namespace {

// NOTE(shess): kFileMagic should not be a byte-wise palindrome, so
// that byte-order changes force corruption.
const int32 kFileMagic = 0x600D71FE;
const int32 kFileVersion = 7;  // SQLite storage was 6...

// Header at the front of the main database file.
struct FileHeader {
  int32 magic, version;
  uint32 add_chunk_count, sub_chunk_count;
  uint32 add_prefix_count, sub_prefix_count;
  uint32 add_hash_count, sub_hash_count;
};

// Header for each chunk in the chunk-accumulation file.
struct ChunkHeader {
  uint32 add_prefix_count, sub_prefix_count;
  uint32 add_hash_count, sub_hash_count;
};

// Rewind the file.  Using fseek(2) because rewind(3) errors are
// weird.
bool FileRewind(FILE* fp) {
  int rv = fseek(fp, 0, SEEK_SET);
  DCHECK_EQ(rv, 0);
  return rv == 0;
}

// Move file read pointer forward by |bytes| relative to current position.
bool FileSkip(size_t bytes, FILE* fp) {
  // Although fseek takes negative values, for this case, we only want
  // to skip forward.
  DCHECK(static_cast<long>(bytes) >= 0);
  if (static_cast<long>(bytes) < 0)
    return false;
  int rv = fseek(fp, static_cast<long>(bytes), SEEK_CUR);
  DCHECK_EQ(rv, 0);
  return rv == 0;
}

// Read an array of |nmemb| items from |fp| into |ptr|, and fold the
// input data into the checksum in |context|, if non-NULL.  Return
// true on success.
template <class T>
bool ReadArray(T* ptr, size_t nmemb, FILE* fp, MD5Context* context) {
  const size_t ret = fread(ptr, sizeof(T), nmemb, fp);
  if (ret != nmemb)
    return false;

  if (context)
    MD5Update(context, ptr, sizeof(T) * nmemb);
  return true;
}

// Write an array of |nmemb| items from |ptr| to |fp|, and fold the
// output data into the checksum in |context|, if non-NULL.  Return
// true on success.
template <class T>
bool WriteArray(const T* ptr, size_t nmemb, FILE* fp, MD5Context* context) {
  const size_t ret = fwrite(ptr, sizeof(T), nmemb, fp);
  if (ret != nmemb)
    return false;

  if (context)
    MD5Update(context, ptr, sizeof(T) * nmemb);

  return true;
}

// Expand |values| to fit |count| new items, read those items from
// |fp| and fold them into the checksum in |context|.  Returns true on
// success.
template <class T>
bool ReadToVector(std::vector<T>* values, size_t count,
                  FILE* fp, MD5Context* context) {
  // Pointers into an empty vector may not be valid.
  if (!count)
    return true;

  // Grab the size for purposes of finding where to read to.  The
  // resize could invalidate any iterator captured here.
  const size_t original_size = values->size();
  values->resize(original_size + count);

  // Sayeth Herb Sutter: Vectors are guaranteed to be contiguous.  So
  // get a pointer to where to read the data to.
  T* ptr = &((*values)[original_size]);
  if (!ReadArray(ptr, count, fp, context)) {
    values->resize(original_size);
    return false;
  }

  return true;
}

// Write all of |values| to |fp|, and fold the data into the checksum
// in |context|, if non-NULL.  Returns true on succsess.
template <class T>
bool WriteVector(const std::vector<T>& values, FILE* fp, MD5Context* context) {
  // Pointers into empty vectors may not be valid.
  if (values.empty())
    return true;

  // Sayeth Herb Sutter: Vectors are guaranteed to be contiguous.  So
  // get a pointer to where to write from.
  const T* ptr = &(values[0]);
  return WriteArray(ptr, values.size(), fp, context);
}

// Read an array of |count| integers and add them to |values|.
// Returns true on success.
bool ReadToChunkSet(std::set<int32>* values, size_t count,
                    FILE* fp, MD5Context* context) {
  if (!count)
    return true;

  std::vector<int32> flat_values;
  if (!ReadToVector(&flat_values, count, fp, context))
    return false;

  values->insert(flat_values.begin(), flat_values.end());
  return true;
}

// Write the contents of |values| as an array of integers.  Returns
// true on success.
bool WriteChunkSet(const std::set<int32>& values,
                   FILE* fp, MD5Context* context) {
  if (values.empty())
    return true;

  const std::vector<int32> flat_values(values.begin(), values.end());
  return WriteVector(flat_values, fp, context);
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

// Sanity-check the header against the file's size to make sure our
// vectors aren't gigantic.  This doubles as a cheap way to detect
// corruption without having to checksum the entire file.
bool FileHeaderSanityCheck(const FilePath& filename,
                           const FileHeader& header) {
  int64 size = 0;
  if (!file_util::GetFileSize(filename, &size))
    return false;

  int64 expected_size = sizeof(FileHeader);
  expected_size += header.add_chunk_count * sizeof(int32);
  expected_size += header.sub_chunk_count * sizeof(int32);
  expected_size += header.add_prefix_count * sizeof(SBAddPrefix);
  expected_size += header.sub_prefix_count * sizeof(SBSubPrefix);
  expected_size += header.add_hash_count * sizeof(SBAddFullHash);
  expected_size += header.sub_hash_count * sizeof(SBSubFullHash);
  expected_size += sizeof(MD5Digest);
  if (size != expected_size)
    return false;

  return true;
}

// This a helper function that reads header to |header|. Returns true if the
// magic number is correct and santiy check passes.
bool ReadAndVerifyHeader(const FilePath& filename,
                         FILE* fp,
                         FileHeader* header,
                         MD5Context* context) {
  if (!ReadArray(header, 1, fp, context))
    return false;
  if (header->magic != kFileMagic || header->version != kFileVersion)
    return false;
  if (!FileHeaderSanityCheck(filename, *header))
    return false;
  return true;
}

}  // namespace

// static
void SafeBrowsingStoreFile::RecordFormatEvent(FormatEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.FormatEvent", event_type, FORMAT_EVENT_MAX);
}

// static
void SafeBrowsingStoreFile::CheckForOriginalAndDelete(
    const FilePath& current_filename) {
  const FilePath original_filename(
      current_filename.DirName().AppendASCII("Safe Browsing"));
  if (file_util::PathExists(original_filename)) {
    int64 size = 0;
    if (file_util::GetFileSize(original_filename, &size)) {
      UMA_HISTOGRAM_COUNTS("SB2.OldDatabaseKilobytes",
                           static_cast<int>(size / 1024));
    }

    if (file_util::Delete(original_filename, false)) {
      RecordFormatEvent(FORMAT_EVENT_DELETED_ORIGINAL);
    } else {
      RecordFormatEvent(FORMAT_EVENT_DELETED_ORIGINAL_FAILED);
    }

    // Just best-effort on the journal file, don't want to get lost in
    // the weeds.
    const FilePath journal_filename(
        current_filename.DirName().AppendASCII("Safe Browsing-journal"));
    file_util::Delete(journal_filename, false);
  }
}

SafeBrowsingStoreFile::SafeBrowsingStoreFile()
    : chunks_written_(0),
      file_(NULL),
      empty_(false),
      corruption_seen_(false) {
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

  // With SQLite support gone, one way to get to this code is if the
  // existing file is a SQLite file.  Make sure the journal file is
  // also removed.
  const FilePath journal_filename(
      filename_.value() + FILE_PATH_LITERAL("-journal"));
  if (file_util::PathExists(journal_filename))
    file_util::Delete(journal_filename, false);

  return true;
}

void SafeBrowsingStoreFile::Init(const FilePath& filename,
                                 Callback0::Type* corruption_callback) {
  filename_ = filename;
  corruption_callback_.reset(corruption_callback);
}

bool SafeBrowsingStoreFile::BeginChunk() {
  return ClearChunkBuffers();
}

bool SafeBrowsingStoreFile::WriteAddPrefix(int32 chunk_id, SBPrefix prefix) {
  add_prefixes_.push_back(SBAddPrefix(chunk_id, prefix));
  return true;
}

bool SafeBrowsingStoreFile::GetAddPrefixes(
   std::vector<SBAddPrefix>* add_prefixes) {
  add_prefixes->clear();

  file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb"));
  if (file.get() == NULL) return false;

  FileHeader header;
  if (!ReadAndVerifyHeader(filename_, file.get(), &header, NULL))
    return OnCorruptDatabase();

  size_t add_prefix_offset = header.add_chunk_count * sizeof(int32) +
      header.sub_chunk_count * sizeof(int32);
  if (!FileSkip(add_prefix_offset, file.get()))
    return false;

  if (!ReadToVector(add_prefixes, header.add_prefix_count, file.get(), NULL))
    return false;

  return true;
}

bool SafeBrowsingStoreFile::WriteAddHash(int32 chunk_id,
                                         base::Time receive_time,
                                         const SBFullHash& full_hash) {
  add_hashes_.push_back(SBAddFullHash(chunk_id, receive_time, full_hash));
  return true;
}

bool SafeBrowsingStoreFile::WriteSubPrefix(int32 chunk_id,
                                           int32 add_chunk_id,
                                           SBPrefix prefix) {
  sub_prefixes_.push_back(SBSubPrefix(chunk_id, add_chunk_id, prefix));
  return true;
}

bool SafeBrowsingStoreFile::WriteSubHash(int32 chunk_id, int32 add_chunk_id,
                                         const SBFullHash& full_hash) {
  sub_hashes_.push_back(SBSubFullHash(chunk_id, add_chunk_id, full_hash));
  return true;
}

bool SafeBrowsingStoreFile::OnCorruptDatabase() {
  if (!corruption_seen_)
    RecordFormatEvent(FORMAT_EVENT_FILE_CORRUPT);
  corruption_seen_ = true;

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

  // Since the following code will already hit the profile looking for
  // database files, this is a reasonable to time delete any old
  // files.
  CheckForOriginalAndDelete(filename_);

  corruption_seen_ = false;

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

  FileHeader header;
  if (!ReadArray(&header, 1, file.get(), NULL))
      return OnCorruptDatabase();

  if (header.magic != kFileMagic || header.version != kFileVersion) {
    if (!strcmp(reinterpret_cast<char*>(&header.magic), "SQLite format 3")) {
      RecordFormatEvent(FORMAT_EVENT_FOUND_SQLITE);
    } else {
      RecordFormatEvent(FORMAT_EVENT_FOUND_UNKNOWN);
    }

    // Close the file so that it can be deleted.
    file.reset();

    return OnCorruptDatabase();
  }

  // TODO(shess): Under POSIX it is possible that this could size a
  // file different from the file which was opened.
  if (!FileHeaderSanityCheck(filename_, header))
    return OnCorruptDatabase();

  // Pull in the chunks-seen data for purposes of implementing
  // |GetAddChunks()| and |GetSubChunks()|.  This data is sent up to
  // the server at the beginning of an update.
  if (!ReadToChunkSet(&add_chunks_cache_, header.add_chunk_count,
                      file.get(), NULL) ||
      !ReadToChunkSet(&sub_chunks_cache_, header.sub_chunk_count,
                      file.get(), NULL))
    return OnCorruptDatabase();

  file_.swap(file);
  new_file_.swap(new_file);
  return true;
}

bool SafeBrowsingStoreFile::FinishChunk() {
  if (!add_prefixes_.size() && !sub_prefixes_.size() &&
      !add_hashes_.size() && !sub_hashes_.size())
    return true;

  ChunkHeader header;
  header.add_prefix_count = add_prefixes_.size();
  header.sub_prefix_count = sub_prefixes_.size();
  header.add_hash_count = add_hashes_.size();
  header.sub_hash_count = sub_hashes_.size();
  if (!WriteArray(&header, 1, new_file_.get(), NULL))
    return false;

  if (!WriteVector(add_prefixes_, new_file_.get(), NULL) ||
      !WriteVector(sub_prefixes_, new_file_.get(), NULL) ||
      !WriteVector(add_hashes_, new_file_.get(), NULL) ||
      !WriteVector(sub_hashes_, new_file_.get(), NULL))
    return false;

  ++chunks_written_;

  // Clear everything to save memory.
  return ClearChunkBuffers();
}

bool SafeBrowsingStoreFile::DoUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    const std::set<SBPrefix>& prefix_misses,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  DCHECK(file_.get() || empty_);
  DCHECK(new_file_.get());
  CHECK(add_prefixes_result);
  CHECK(add_full_hashes_result);

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBAddFullHash> add_full_hashes;
  std::vector<SBSubFullHash> sub_full_hashes;

  // Read original data into the vectors.
  if (!empty_) {
    DCHECK(file_.get());

    if (!FileRewind(file_.get()))
      return OnCorruptDatabase();

    MD5Context context;
    MD5Init(&context);

    // Read the file header and make sure it looks right.
    FileHeader header;
    if (!ReadAndVerifyHeader(filename_, file_.get(), &header, &context))
      return OnCorruptDatabase();

    // Re-read the chunks-seen data to get to the later data in the
    // file and calculate the checksum.  No new elements should be
    // added to the sets.
    if (!ReadToChunkSet(&add_chunks_cache_, header.add_chunk_count,
                        file_.get(), &context) ||
        !ReadToChunkSet(&sub_chunks_cache_, header.sub_chunk_count,
                        file_.get(), &context))
      return OnCorruptDatabase();

    if (!ReadToVector(&add_prefixes, header.add_prefix_count,
                      file_.get(), &context) ||
        !ReadToVector(&sub_prefixes, header.sub_prefix_count,
                      file_.get(), &context) ||
        !ReadToVector(&add_full_hashes, header.add_hash_count,
                      file_.get(), &context) ||
        !ReadToVector(&sub_full_hashes, header.sub_hash_count,
                      file_.get(), &context))
      return OnCorruptDatabase();

    // Calculate the digest to this point.
    MD5Digest calculated_digest;
    MD5Final(&calculated_digest, &context);

    // Read the stored checksum and verify it.
    MD5Digest file_digest;
    if (!ReadArray(&file_digest, 1, file_.get(), NULL))
      return OnCorruptDatabase();

    if (0 != memcmp(&file_digest, &calculated_digest, sizeof(file_digest)))
      return OnCorruptDatabase();

    // Close the file so we can later rename over it.
    file_.reset();
  }
  DCHECK(!file_.get());

  // Rewind the temporary storage.
  if (!FileRewind(new_file_.get()))
    return false;

  // Get chunk file's size for validating counts.
  int64 size = 0;
  if (!file_util::GetFileSize(TemporaryFileForFilename(filename_), &size))
    return OnCorruptDatabase();

  // Append the accumulated chunks onto the vectors read from |file_|.
  for (int i = 0; i < chunks_written_; ++i) {
    ChunkHeader header;

    int64 ofs = ftell(new_file_.get());
    if (ofs == -1)
      return false;

    if (!ReadArray(&header, 1, new_file_.get(), NULL))
      return false;

    // As a safety measure, make sure that the header describes a sane
    // chunk, given the remaining file size.
    int64 expected_size = ofs + sizeof(ChunkHeader);
    expected_size += header.add_prefix_count * sizeof(SBAddPrefix);
    expected_size += header.sub_prefix_count * sizeof(SBSubPrefix);
    expected_size += header.add_hash_count * sizeof(SBAddFullHash);
    expected_size += header.sub_hash_count * sizeof(SBSubFullHash);
    if (expected_size > size)
      return false;

    // TODO(shess): If the vectors were kept sorted, then this code
    // could use std::inplace_merge() to merge everything together in
    // sorted order.  That might still be slower than just sorting at
    // the end if there were a large number of chunks.  In that case
    // some sort of recursive binary merge might be in order (merge
    // chunks pairwise, merge those chunks pairwise, and so on, then
    // merge the result with the main list).
    if (!ReadToVector(&add_prefixes, header.add_prefix_count,
                      new_file_.get(), NULL) ||
        !ReadToVector(&sub_prefixes, header.sub_prefix_count,
                      new_file_.get(), NULL) ||
        !ReadToVector(&add_full_hashes, header.add_hash_count,
                      new_file_.get(), NULL) ||
        !ReadToVector(&sub_full_hashes, header.sub_hash_count,
                      new_file_.get(), NULL))
      return false;
  }

  // Append items from |pending_adds|.
  add_full_hashes.insert(add_full_hashes.end(),
                         pending_adds.begin(), pending_adds.end());

  // Check how often a prefix was checked which wasn't in the
  // database.
  SBCheckPrefixMisses(add_prefixes, prefix_misses);

  // Knock the subs from the adds and process deleted chunks.
  SBProcessSubs(&add_prefixes, &sub_prefixes,
                &add_full_hashes, &sub_full_hashes,
                add_del_cache_, sub_del_cache_);

  // We no longer need to track deleted chunks.
  DeleteChunksFromSet(add_del_cache_, &add_chunks_cache_);
  DeleteChunksFromSet(sub_del_cache_, &sub_chunks_cache_);

  // Write the new data to new_file_.
  if (!FileRewind(new_file_.get()))
    return false;

  MD5Context context;
  MD5Init(&context);

  // Write a file header.
  FileHeader header;
  header.magic = kFileMagic;
  header.version = kFileVersion;
  header.add_chunk_count = add_chunks_cache_.size();
  header.sub_chunk_count = sub_chunks_cache_.size();
  header.add_prefix_count = add_prefixes.size();
  header.sub_prefix_count = sub_prefixes.size();
  header.add_hash_count = add_full_hashes.size();
  header.sub_hash_count = sub_full_hashes.size();
  if (!WriteArray(&header, 1, new_file_.get(), &context))
    return false;

  // Write all the chunk data.
  if (!WriteChunkSet(add_chunks_cache_, new_file_.get(), &context) ||
      !WriteChunkSet(sub_chunks_cache_, new_file_.get(), &context) ||
      !WriteVector(add_prefixes, new_file_.get(), &context) ||
      !WriteVector(sub_prefixes, new_file_.get(), &context) ||
      !WriteVector(add_full_hashes, new_file_.get(), &context) ||
      !WriteVector(sub_full_hashes, new_file_.get(), &context))
    return false;

  // Write the checksum at the end.
  MD5Digest digest;
  MD5Final(&digest, &context);
  if (!WriteArray(&digest, 1, new_file_.get(), NULL))
    return false;

  // Trim any excess left over from the temporary chunk data.
  if (!file_util::TruncateFile(new_file_.get()))
    return false;

  // Close the file handle and swizzle the file into place.
  new_file_.reset();
  if (!file_util::Delete(filename_, false) &&
      file_util::PathExists(filename_))
    return false;

  const FilePath new_filename = TemporaryFileForFilename(filename_);
  if (!file_util::Move(new_filename, filename_))
    return false;

  // Record counts before swapping to caller.
  UMA_HISTOGRAM_COUNTS("SB2.AddPrefixes", add_prefixes.size());
  UMA_HISTOGRAM_COUNTS("SB2.SubPrefixes", sub_prefixes.size());

  // Pass the resulting data off to the caller.
  add_prefixes_result->swap(add_prefixes);
  add_full_hashes_result->swap(add_full_hashes);

  return true;
}

bool SafeBrowsingStoreFile::FinishUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    const std::set<SBPrefix>& prefix_misses,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  DCHECK(add_prefixes_result);
  DCHECK(add_full_hashes_result);

  bool ret = DoUpdate(pending_adds, prefix_misses,
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

void SafeBrowsingStoreFile::SetAddChunk(int32 chunk_id) {
  add_chunks_cache_.insert(chunk_id);
}

bool SafeBrowsingStoreFile::CheckAddChunk(int32 chunk_id) {
  return add_chunks_cache_.count(chunk_id) > 0;
}

void SafeBrowsingStoreFile::GetAddChunks(std::vector<int32>* out) {
  out->clear();
  out->insert(out->end(), add_chunks_cache_.begin(), add_chunks_cache_.end());
}

void SafeBrowsingStoreFile::SetSubChunk(int32 chunk_id) {
  sub_chunks_cache_.insert(chunk_id);
}

bool SafeBrowsingStoreFile::CheckSubChunk(int32 chunk_id) {
  return sub_chunks_cache_.count(chunk_id) > 0;
}

void SafeBrowsingStoreFile::GetSubChunks(std::vector<int32>* out) {
  out->clear();
  out->insert(out->end(), sub_chunks_cache_.begin(), sub_chunks_cache_.end());
}

void SafeBrowsingStoreFile::DeleteAddChunk(int32 chunk_id) {
  add_del_cache_.insert(chunk_id);
}

void SafeBrowsingStoreFile::DeleteSubChunk(int32 chunk_id) {
  sub_del_cache_.insert(chunk_id);
}
