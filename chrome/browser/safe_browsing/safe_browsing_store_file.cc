// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

#include "base/md5.h"
#include "base/metrics/histogram.h"

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

// Read from |fp| into |item|, and fold the input data into the
// checksum in |context|, if non-NULL.  Return true on success.
template <class T>
bool ReadItem(T* item, FILE* fp, base::MD5Context* context) {
  const size_t ret = fread(item, sizeof(T), 1, fp);
  if (ret != 1)
    return false;

  if (context) {
    base::MD5Update(context,
                    base::StringPiece(reinterpret_cast<char*>(item),
                                      sizeof(T)));
  }
  return true;
}

// Write |item| to |fp|, and fold the output data into the checksum in
// |context|, if non-NULL.  Return true on success.
template <class T>
bool WriteItem(const T& item, FILE* fp, base::MD5Context* context) {
  const size_t ret = fwrite(&item, sizeof(T), 1, fp);
  if (ret != 1)
    return false;

  if (context) {
    base::MD5Update(context,
                    base::StringPiece(reinterpret_cast<const char*>(&item),
                                      sizeof(T)));
  }

  return true;
}

// Read |count| items into |values| from |fp|, and fold them into the
// checksum in |context|.  Returns true on success.
template <typename CT>
bool ReadToContainer(CT* values, size_t count, FILE* fp,
                     base::MD5Context* context) {
  if (!count)
    return true;

  for (size_t i = 0; i < count; ++i) {
    typename CT::value_type value;
    if (!ReadItem(&value, fp, context))
      return false;

    // push_back() is more obvious, but coded this way std::set can
    // also be read.
    values->insert(values->end(), value);
  }

  return true;
}

// Write all of |values| to |fp|, and fold the data into the checksum
// in |context|, if non-NULL.  Returns true on succsess.
template <typename CT>
bool WriteContainer(const CT& values, FILE* fp,
                    base::MD5Context* context) {
  if (values.empty())
    return true;

  for (typename CT::const_iterator iter = values.begin();
       iter != values.end(); ++iter) {
    if (!WriteItem(*iter, fp, context))
      return false;
  }
  return true;
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
bool FileHeaderSanityCheck(const base::FilePath& filename,
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
  expected_size += sizeof(base::MD5Digest);
  if (size != expected_size)
    return false;

  return true;
}

// This a helper function that reads header to |header|. Returns true if the
// magic number is correct and santiy check passes.
bool ReadAndVerifyHeader(const base::FilePath& filename,
                         FILE* fp,
                         FileHeader* header,
                         base::MD5Context* context) {
  if (!ReadItem(header, fp, context))
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
    const base::FilePath& current_filename) {
  const base::FilePath original_filename(
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
    const base::FilePath journal_filename(
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

  const base::FilePath new_filename = TemporaryFileForFilename(filename_);
  if (!file_util::Delete(new_filename, false) &&
      file_util::PathExists(new_filename)) {
    NOTREACHED();
    return false;
  }

  // With SQLite support gone, one way to get to this code is if the
  // existing file is a SQLite file.  Make sure the journal file is
  // also removed.
  const base::FilePath journal_filename(
      filename_.value() + FILE_PATH_LITERAL("-journal"));
  if (file_util::PathExists(journal_filename))
    file_util::Delete(journal_filename, false);

  return true;
}

bool SafeBrowsingStoreFile::CheckValidity() {
  // The file was either empty or never opened.  The empty case is
  // presumed not to be invalid.  The never-opened case can happen if
  // BeginUpdate() fails for any databases, and should already have
  // caused the corruption callback to fire.
  if (!file_.get())
    return true;

  if (!FileRewind(file_.get()))
    return OnCorruptDatabase();

  int64 size = 0;
  if (!file_util::GetFileSize(filename_, &size))
    return OnCorruptDatabase();

  base::MD5Context context;
  base::MD5Init(&context);

  // Read everything except the final digest.
  size_t bytes_left = static_cast<size_t>(size);
  CHECK(size == static_cast<int64>(bytes_left));
  if (bytes_left < sizeof(base::MD5Digest))
    return OnCorruptDatabase();
  bytes_left -= sizeof(base::MD5Digest);

  // Fold the contents of the file into the checksum.
  while (bytes_left > 0) {
    char buf[4096];
    const size_t c = std::min(sizeof(buf), bytes_left);
    const size_t ret = fread(buf, 1, c, file_.get());

    // The file's size changed while reading, give up.
    if (ret != c)
      return OnCorruptDatabase();
    base::MD5Update(&context, base::StringPiece(buf, c));
    bytes_left -= c;
  }

  // Calculate the digest to this point.
  base::MD5Digest calculated_digest;
  base::MD5Final(&calculated_digest, &context);

  // Read the stored digest and verify it.
  base::MD5Digest file_digest;
  if (!ReadItem(&file_digest, file_.get(), NULL))
    return OnCorruptDatabase();
  if (0 != memcmp(&file_digest, &calculated_digest, sizeof(file_digest))) {
    RecordFormatEvent(FORMAT_EVENT_VALIDITY_CHECKSUM_FAILURE);
    return OnCorruptDatabase();
  }

  return true;
}

void SafeBrowsingStoreFile::Init(
    const base::FilePath& filename,
    const base::Closure& corruption_callback
) {
  filename_ = filename;
  corruption_callback_ = corruption_callback;
}

bool SafeBrowsingStoreFile::BeginChunk() {
  return ClearChunkBuffers();
}

bool SafeBrowsingStoreFile::WriteAddPrefix(int32 chunk_id, SBPrefix prefix) {
  add_prefixes_.push_back(SBAddPrefix(chunk_id, prefix));
  return true;
}

bool SafeBrowsingStoreFile::GetAddPrefixes(SBAddPrefixes* add_prefixes) {
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

  if (!ReadToContainer(add_prefixes, header.add_prefix_count, file.get(), NULL))
    return false;

  return true;
}

bool SafeBrowsingStoreFile::GetAddFullHashes(
    std::vector<SBAddFullHash>* add_full_hashes) {
  add_full_hashes->clear();

  file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb"));
  if (file.get() == NULL) return false;

  FileHeader header;
  if (!ReadAndVerifyHeader(filename_, file.get(), &header, NULL))
    return OnCorruptDatabase();

  size_t offset =
      header.add_chunk_count * sizeof(int32) +
      header.sub_chunk_count * sizeof(int32) +
      header.add_prefix_count * sizeof(SBAddPrefix) +
      header.sub_prefix_count * sizeof(SBSubPrefix);
  if (!FileSkip(offset, file.get()))
    return false;

  return ReadToContainer(add_full_hashes,
                         header.add_hash_count,
                         file.get(),
                         NULL);
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

  corruption_callback_.Run();

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

  const base::FilePath new_filename = TemporaryFileForFilename(filename_);
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
  if (!ReadItem(&header, file.get(), NULL))
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
  if (!ReadToContainer(&add_chunks_cache_, header.add_chunk_count,
                       file.get(), NULL) ||
      !ReadToContainer(&sub_chunks_cache_, header.sub_chunk_count,
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
  if (!WriteItem(header, new_file_.get(), NULL))
    return false;

  if (!WriteContainer(add_prefixes_, new_file_.get(), NULL) ||
      !WriteContainer(sub_prefixes_, new_file_.get(), NULL) ||
      !WriteContainer(add_hashes_, new_file_.get(), NULL) ||
      !WriteContainer(sub_hashes_, new_file_.get(), NULL))
    return false;

  ++chunks_written_;

  // Clear everything to save memory.
  return ClearChunkBuffers();
}

bool SafeBrowsingStoreFile::DoUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    const std::set<SBPrefix>& prefix_misses,
    SBAddPrefixes* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  DCHECK(file_.get() || empty_);
  DCHECK(new_file_.get());
  CHECK(add_prefixes_result);
  CHECK(add_full_hashes_result);

  SBAddPrefixes add_prefixes;
  SBSubPrefixes sub_prefixes;
  std::vector<SBAddFullHash> add_full_hashes;
  std::vector<SBSubFullHash> sub_full_hashes;

  // Read original data into the vectors.
  if (!empty_) {
    DCHECK(file_.get());

    if (!FileRewind(file_.get()))
      return OnCorruptDatabase();

    base::MD5Context context;
    base::MD5Init(&context);

    // Read the file header and make sure it looks right.
    FileHeader header;
    if (!ReadAndVerifyHeader(filename_, file_.get(), &header, &context))
      return OnCorruptDatabase();

    // Re-read the chunks-seen data to get to the later data in the
    // file and calculate the checksum.  No new elements should be
    // added to the sets.
    if (!ReadToContainer(&add_chunks_cache_, header.add_chunk_count,
                         file_.get(), &context) ||
        !ReadToContainer(&sub_chunks_cache_, header.sub_chunk_count,
                         file_.get(), &context))
      return OnCorruptDatabase();

    if (!ReadToContainer(&add_prefixes, header.add_prefix_count,
                         file_.get(), &context) ||
        !ReadToContainer(&sub_prefixes, header.sub_prefix_count,
                         file_.get(), &context) ||
        !ReadToContainer(&add_full_hashes, header.add_hash_count,
                         file_.get(), &context) ||
        !ReadToContainer(&sub_full_hashes, header.sub_hash_count,
                         file_.get(), &context))
      return OnCorruptDatabase();

    // Calculate the digest to this point.
    base::MD5Digest calculated_digest;
    base::MD5Final(&calculated_digest, &context);

    // Read the stored checksum and verify it.
    base::MD5Digest file_digest;
    if (!ReadItem(&file_digest, file_.get(), NULL))
      return OnCorruptDatabase();

    if (0 != memcmp(&file_digest, &calculated_digest, sizeof(file_digest))) {
      RecordFormatEvent(FORMAT_EVENT_UPDATE_CHECKSUM_FAILURE);
      return OnCorruptDatabase();
    }

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

  // Track update size to answer questions at http://crbug.com/72216 .
  // Log small updates as 1k so that the 0 (underflow) bucket can be
  // used for "empty" in SafeBrowsingDatabase.
  UMA_HISTOGRAM_COUNTS("SB2.DatabaseUpdateKilobytes",
                       std::max(static_cast<int>(size / 1024), 1));

  // Append the accumulated chunks onto the vectors read from |file_|.
  for (int i = 0; i < chunks_written_; ++i) {
    ChunkHeader header;

    int64 ofs = ftell(new_file_.get());
    if (ofs == -1)
      return false;

    if (!ReadItem(&header, new_file_.get(), NULL))
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
    if (!ReadToContainer(&add_prefixes, header.add_prefix_count,
                         new_file_.get(), NULL) ||
        !ReadToContainer(&sub_prefixes, header.sub_prefix_count,
                         new_file_.get(), NULL) ||
        !ReadToContainer(&add_full_hashes, header.add_hash_count,
                         new_file_.get(), NULL) ||
        !ReadToContainer(&sub_full_hashes, header.sub_hash_count,
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

  base::MD5Context context;
  base::MD5Init(&context);

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
  if (!WriteItem(header, new_file_.get(), &context))
    return false;

  // Write all the chunk data.
  if (!WriteContainer(add_chunks_cache_, new_file_.get(), &context) ||
      !WriteContainer(sub_chunks_cache_, new_file_.get(), &context) ||
      !WriteContainer(add_prefixes, new_file_.get(), &context) ||
      !WriteContainer(sub_prefixes, new_file_.get(), &context) ||
      !WriteContainer(add_full_hashes, new_file_.get(), &context) ||
      !WriteContainer(sub_full_hashes, new_file_.get(), &context))
    return false;

  // Write the checksum at the end.
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  if (!WriteItem(digest, new_file_.get(), NULL))
    return false;

  // Trim any excess left over from the temporary chunk data.
  if (!file_util::TruncateFile(new_file_.get()))
    return false;

  // Close the file handle and swizzle the file into place.
  new_file_.reset();
  if (!file_util::Delete(filename_, false) &&
      file_util::PathExists(filename_))
    return false;

  const base::FilePath new_filename = TemporaryFileForFilename(filename_);
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
    SBAddPrefixes* add_prefixes_result,
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
