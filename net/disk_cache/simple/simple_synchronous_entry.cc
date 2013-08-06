// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_synchronous_entry.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"

using base::kInvalidPlatformFileValue;
using base::ClosePlatformFile;
using base::FilePath;
using base::GetPlatformFileInfo;
using base::PlatformFileError;
using base::PlatformFileInfo;
using base::PLATFORM_FILE_CREATE;
using base::PLATFORM_FILE_ERROR_EXISTS;
using base::PLATFORM_FILE_OK;
using base::PLATFORM_FILE_OPEN;
using base::PLATFORM_FILE_READ;
using base::PLATFORM_FILE_WRITE;
using base::ReadPlatformFile;
using base::Time;
using base::TruncatePlatformFile;
using base::WritePlatformFile;

namespace {

// Used in histograms, please only add entries at the end.
enum OpenEntryResult {
  OPEN_ENTRY_SUCCESS = 0,
  OPEN_ENTRY_PLATFORM_FILE_ERROR = 1,
  OPEN_ENTRY_CANT_READ_HEADER = 2,
  OPEN_ENTRY_BAD_MAGIC_NUMBER = 3,
  OPEN_ENTRY_BAD_VERSION = 4,
  OPEN_ENTRY_CANT_READ_KEY = 5,
  // OPEN_ENTRY_KEY_MISMATCH = 6, Deprecated.
  OPEN_ENTRY_KEY_HASH_MISMATCH = 7,
  OPEN_ENTRY_MAX = 8,
};

// Used in histograms, please only add entries at the end.
enum CreateEntryResult {
  CREATE_ENTRY_SUCCESS = 0,
  CREATE_ENTRY_PLATFORM_FILE_ERROR = 1,
  CREATE_ENTRY_CANT_WRITE_HEADER = 2,
  CREATE_ENTRY_CANT_WRITE_KEY = 3,
  CREATE_ENTRY_MAX = 4,
};

// Used in histograms, please only add entries at the end.
enum WriteResult {
  WRITE_RESULT_SUCCESS = 0,
  WRITE_RESULT_PRETRUNCATE_FAILURE,
  WRITE_RESULT_WRITE_FAILURE,
  WRITE_RESULT_TRUNCATE_FAILURE,
  WRITE_RESULT_MAX,
};

// Used in histograms, please only add entries at the end.
enum CheckEOFResult {
  CHECK_EOF_RESULT_SUCCESS,
  CHECK_EOF_RESULT_READ_FAILURE,
  CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH,
  CHECK_EOF_RESULT_CRC_MISMATCH,
  CHECK_EOF_RESULT_MAX,
};

// Used in histograms, please only add entries at the end.
enum CloseResult {
  CLOSE_RESULT_SUCCESS,
  CLOSE_RESULT_WRITE_FAILURE,
};

void RecordSyncOpenResult(OpenEntryResult result, bool had_index) {
  DCHECK_GT(OPEN_ENTRY_MAX, result);
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncOpenResult", result, OPEN_ENTRY_MAX);
  if (had_index) {
    UMA_HISTOGRAM_ENUMERATION(
        "SimpleCache.SyncOpenResult_WithIndex", result, OPEN_ENTRY_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "SimpleCache.SyncOpenResult_WithoutIndex", result, OPEN_ENTRY_MAX);
  }
}

void RecordSyncCreateResult(CreateEntryResult result, bool had_index) {
  DCHECK_GT(CREATE_ENTRY_MAX, result);
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncCreateResult", result, CREATE_ENTRY_MAX);
  if (had_index) {
    UMA_HISTOGRAM_ENUMERATION(
        "SimpleCache.SyncCreateResult_WithIndex", result, CREATE_ENTRY_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "SimpleCache.SyncCreateResult_WithoutIndex", result, CREATE_ENTRY_MAX);
  }
}

void RecordWriteResult(WriteResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncWriteResult", result, WRITE_RESULT_MAX);
}

void RecordCheckEOFResult(CheckEOFResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncCheckEOFResult", result, CHECK_EOF_RESULT_MAX);
}

void RecordCloseResult(CloseResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncCloseResult", result, WRITE_RESULT_MAX);
}

}  // namespace

namespace disk_cache {

using simple_util::ConvertEntryHashKeyToHexString;
using simple_util::GetEntryHashKey;
using simple_util::GetFilenameFromEntryHashAndIndex;
using simple_util::GetDataSizeFromKeyAndFileSize;
using simple_util::GetFileSizeFromKeyAndDataSize;
using simple_util::GetFileOffsetFromKeyAndDataOffset;

SimpleEntryStat::SimpleEntryStat() {}

SimpleEntryStat::SimpleEntryStat(base::Time last_used_p,
                                 base::Time last_modified_p,
                                 const int32 data_size_p[])
    : last_used(last_used_p),
      last_modified(last_modified_p) {
  memcpy(data_size, data_size_p, sizeof(data_size));
}

SimpleEntryCreationResults::SimpleEntryCreationResults(
    SimpleEntryStat entry_stat)
    : sync_entry(NULL),
      entry_stat(entry_stat),
      result(net::OK) {
}

SimpleEntryCreationResults::~SimpleEntryCreationResults() {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord() : index(-1),
                                                 has_crc32(false),
                                                 data_crc32(0) {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord(int index_p,
                                             bool has_crc32_p,
                                             uint32 data_crc32_p)
    : index(index_p),
      has_crc32(has_crc32_p),
      data_crc32(data_crc32_p) {}

SimpleSynchronousEntry::EntryOperationData::EntryOperationData(int index_p,
                                                               int offset_p,
                                                               int buf_len_p)
    : index(index_p),
      offset(offset_p),
      buf_len(buf_len_p) {}

SimpleSynchronousEntry::EntryOperationData::EntryOperationData(int index_p,
                                                               int offset_p,
                                                               int buf_len_p,
                                                               bool truncate_p)
    : index(index_p),
      offset(offset_p),
      buf_len(buf_len_p),
      truncate(truncate_p) {}

// static
void SimpleSynchronousEntry::OpenEntry(
    const FilePath& path,
    const uint64 entry_hash,
    bool had_index,
    SimpleEntryCreationResults *out_results) {
  SimpleSynchronousEntry* sync_entry = new SimpleSynchronousEntry(path, "",
                                                                  entry_hash);
  out_results->result = sync_entry->InitializeForOpen(
      had_index, &out_results->entry_stat);
  if (out_results->result != net::OK) {
    sync_entry->Doom();
    delete sync_entry;
    out_results->sync_entry = NULL;
    return;
  }
  out_results->sync_entry = sync_entry;
}

// static
void SimpleSynchronousEntry::CreateEntry(
    const FilePath& path,
    const std::string& key,
    const uint64 entry_hash,
    bool had_index,
    SimpleEntryCreationResults *out_results) {
  DCHECK_EQ(entry_hash, GetEntryHashKey(key));
  SimpleSynchronousEntry* sync_entry = new SimpleSynchronousEntry(path, key,
                                                                  entry_hash);
  out_results->result = sync_entry->InitializeForCreate(
      had_index, &out_results->entry_stat);
  if (out_results->result != net::OK) {
    if (out_results->result != net::ERR_FILE_EXISTS)
      sync_entry->Doom();
    delete sync_entry;
    out_results->sync_entry = NULL;
    return;
  }
  out_results->sync_entry = sync_entry;
}

// TODO(gavinp): Move this function to its correct location in this .cc file.
// static
bool SimpleSynchronousEntry::DeleteFilesForEntryHash(
    const FilePath& path,
    const uint64 entry_hash) {
  bool result = true;
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    FilePath to_delete = path.AppendASCII(
        GetFilenameFromEntryHashAndIndex(entry_hash, i));
    if (!base::DeleteFile(to_delete, false)) {
      result = false;
      DLOG(ERROR) << "Could not delete " << to_delete.MaybeAsASCII();
    }
  }
  return result;
}

// static
void SimpleSynchronousEntry::DoomEntry(
    const FilePath& path,
    const std::string& key,
    uint64 entry_hash,
    int* out_result) {
  DCHECK_EQ(entry_hash, GetEntryHashKey(key));
  bool deleted_well = DeleteFilesForEntryHash(path, entry_hash);
  *out_result = deleted_well ? net::OK : net::ERR_FAILED;
}

// static
int SimpleSynchronousEntry::DoomEntrySet(
    scoped_ptr<std::vector<uint64> > key_hashes,
    const FilePath& path) {
  const size_t did_delete_count = std::count_if(
      key_hashes->begin(), key_hashes->end(), std::bind1st(
          std::ptr_fun(SimpleSynchronousEntry::DeleteFilesForEntryHash), path));
  return (did_delete_count == key_hashes->size()) ? net::OK : net::ERR_FAILED;
}

void SimpleSynchronousEntry::ReadData(const EntryOperationData& in_entry_op,
                                      net::IOBuffer* out_buf,
                                      uint32* out_crc32,
                                      base::Time* out_last_used,
                                      int* out_result) const {
  DCHECK(initialized_);
  int64 file_offset =
      GetFileOffsetFromKeyAndDataOffset(key_, in_entry_op.offset);
  int bytes_read = ReadPlatformFile(files_[in_entry_op.index],
                                    file_offset,
                                    out_buf->data(),
                                    in_entry_op.buf_len);
  if (bytes_read > 0) {
    *out_last_used = Time::Now();
    *out_crc32 = crc32(crc32(0L, Z_NULL, 0),
                       reinterpret_cast<const Bytef*>(out_buf->data()),
                       bytes_read);
  }
  if (bytes_read >= 0) {
    *out_result = bytes_read;
  } else {
    *out_result = net::ERR_CACHE_READ_FAILURE;
    Doom();
  }
}

void SimpleSynchronousEntry::WriteData(const EntryOperationData& in_entry_op,
                                       net::IOBuffer* in_buf,
                                       SimpleEntryStat* out_entry_stat,
                                       int* out_result) const {
  DCHECK(initialized_);
  int index = in_entry_op.index;
  int offset = in_entry_op.offset;
  int buf_len = in_entry_op.buf_len;
  int truncate = in_entry_op.truncate;

  bool extending_by_write = offset + buf_len > out_entry_stat->data_size[index];
  if (extending_by_write) {
    // We are extending the file, and need to insure the EOF record is zeroed.
    const int64 file_eof_offset = GetFileOffsetFromKeyAndDataOffset(
        key_, out_entry_stat->data_size[index]);
    if (!TruncatePlatformFile(files_[index], file_eof_offset)) {
      RecordWriteResult(WRITE_RESULT_PRETRUNCATE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  const int64 file_offset = GetFileOffsetFromKeyAndDataOffset(key_, offset);
  if (buf_len > 0) {
    if (WritePlatformFile(
            files_[index], file_offset, in_buf->data(), buf_len) != buf_len) {
      RecordWriteResult(WRITE_RESULT_WRITE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  if (!truncate && (buf_len > 0 || !extending_by_write)) {
    out_entry_stat->data_size[index] =
        std::max(out_entry_stat->data_size[index], offset + buf_len);
  } else {
    if (!TruncatePlatformFile(files_[index], file_offset + buf_len)) {
      RecordWriteResult(WRITE_RESULT_TRUNCATE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    out_entry_stat->data_size[index] = offset + buf_len;
  }

  RecordWriteResult(WRITE_RESULT_SUCCESS);
  out_entry_stat->last_used = out_entry_stat->last_modified = Time::Now();
  *out_result = buf_len;
}

void SimpleSynchronousEntry::CheckEOFRecord(int index,
                                            int32 data_size,
                                            uint32 expected_crc32,
                                            int* out_result) const {
  DCHECK(initialized_);

  SimpleFileEOF eof_record;
  int64 file_offset = GetFileOffsetFromKeyAndDataOffset(key_, data_size);
  if (ReadPlatformFile(files_[index],
                       file_offset,
                       reinterpret_cast<char*>(&eof_record),
                       sizeof(eof_record)) != sizeof(eof_record)) {
    RecordCheckEOFResult(CHECK_EOF_RESULT_READ_FAILURE);
    Doom();
    *out_result = net::ERR_CACHE_CHECKSUM_READ_FAILURE;
    return;
  }

  if (eof_record.final_magic_number != kSimpleFinalMagicNumber) {
    RecordCheckEOFResult(CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH);
    DLOG(INFO) << "eof record had bad magic number.";
    Doom();
    *out_result = net::ERR_CACHE_CHECKSUM_READ_FAILURE;
    return;
  }

  const bool has_crc = (eof_record.flags & SimpleFileEOF::FLAG_HAS_CRC32) ==
                       SimpleFileEOF::FLAG_HAS_CRC32;
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.SyncCheckEOFHasCrc", has_crc);
  if (has_crc && eof_record.data_crc32 != expected_crc32) {
    RecordCheckEOFResult(CHECK_EOF_RESULT_CRC_MISMATCH);
    DLOG(INFO) << "eof record had bad crc.";
    Doom();
    *out_result = net::ERR_CACHE_CHECKSUM_MISMATCH;
    return;
  }

  RecordCheckEOFResult(CHECK_EOF_RESULT_SUCCESS);
  *out_result = net::OK;
}

void SimpleSynchronousEntry::Close(
    const SimpleEntryStat& entry_stat,
    scoped_ptr<std::vector<CRCRecord> > crc32s_to_write) {
  for (std::vector<CRCRecord>::const_iterator it = crc32s_to_write->begin();
       it != crc32s_to_write->end(); ++it) {
    SimpleFileEOF eof_record;
    eof_record.final_magic_number = kSimpleFinalMagicNumber;
    eof_record.flags = 0;
    if (it->has_crc32)
      eof_record.flags |= SimpleFileEOF::FLAG_HAS_CRC32;
    eof_record.data_crc32 = it->data_crc32;
    int64 file_offset = GetFileOffsetFromKeyAndDataOffset(
        key_, entry_stat.data_size[it->index]);
    if (WritePlatformFile(files_[it->index],
                          file_offset,
                          reinterpret_cast<const char*>(&eof_record),
                          sizeof(eof_record)) != sizeof(eof_record)) {
      RecordCloseResult(CLOSE_RESULT_WRITE_FAILURE);
      DLOG(INFO) << "Could not write eof record.";
      Doom();
      break;
    }
    const int64 file_size = file_offset + sizeof(eof_record);
    UMA_HISTOGRAM_CUSTOM_COUNTS("SimpleCache.LastClusterSize",
                                file_size % 4096, 0, 4097, 50);
    const int64 cluster_loss = file_size % 4096 ? 4096 - file_size % 4096 : 0;
    UMA_HISTOGRAM_PERCENTAGE("SimpleCache.LastClusterLossPercent",
                             cluster_loss * 100 / (cluster_loss + file_size));
  }

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    bool did_close_file = ClosePlatformFile(files_[i]);
    CHECK(did_close_file);
  }
  RecordCloseResult(CLOSE_RESULT_SUCCESS);
  have_open_files_ = false;
  delete this;
}

SimpleSynchronousEntry::SimpleSynchronousEntry(const FilePath& path,
                                               const std::string& key,
                                               const uint64 entry_hash)
    : path_(path),
      entry_hash_(entry_hash),
      key_(key),
      have_open_files_(false),
      initialized_(false) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    files_[i] = kInvalidPlatformFileValue;
  }
}

SimpleSynchronousEntry::~SimpleSynchronousEntry() {
  DCHECK(!(have_open_files_ && initialized_));
  if (have_open_files_)
    CloseFiles();
}

bool SimpleSynchronousEntry::OpenOrCreateFiles(
    bool create,
    bool had_index,
    SimpleEntryStat* out_entry_stat) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    FilePath filename = path_.AppendASCII(
        GetFilenameFromEntryHashAndIndex(entry_hash_, i));
    int flags = PLATFORM_FILE_READ | PLATFORM_FILE_WRITE;
    if (create)
      flags |= PLATFORM_FILE_CREATE;
    else
      flags |= PLATFORM_FILE_OPEN;
    PlatformFileError error;
    files_[i] = CreatePlatformFile(filename, flags, NULL, &error);
    if (error != PLATFORM_FILE_OK) {
      // TODO(ttuttle,gavinp): Remove one each of these triplets of histograms.
      // We can calculate the third as the sum or difference of the other two.
      if (create) {
        RecordSyncCreateResult(CREATE_ENTRY_PLATFORM_FILE_ERROR, had_index);
        UMA_HISTOGRAM_ENUMERATION("SimpleCache.SyncCreatePlatformFileError",
                                  -error, -base::PLATFORM_FILE_ERROR_MAX);
        if (had_index) {
          UMA_HISTOGRAM_ENUMERATION(
              "SimpleCache.SyncCreatePlatformFileError_WithIndex",
              -error, -base::PLATFORM_FILE_ERROR_MAX);
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              "SimpleCache.SyncCreatePlatformFileError_WithoutIndex",
              -error, -base::PLATFORM_FILE_ERROR_MAX);
        }
      } else {
        RecordSyncOpenResult(OPEN_ENTRY_PLATFORM_FILE_ERROR, had_index);
        UMA_HISTOGRAM_ENUMERATION("SimpleCache.SyncOpenPlatformFileError",
                                  -error, -base::PLATFORM_FILE_ERROR_MAX);
        if (had_index) {
          UMA_HISTOGRAM_ENUMERATION(
              "SimpleCache.SyncOpenPlatformFileError_WithIndex",
              -error, -base::PLATFORM_FILE_ERROR_MAX);
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              "SimpleCache.SyncOpenPlatformFileError_WithoutIndex",
              -error, -base::PLATFORM_FILE_ERROR_MAX);
        }
      }
      while (--i >= 0) {
        bool ALLOW_UNUSED did_close = ClosePlatformFile(files_[i]);
        DLOG_IF(INFO, !did_close) << "Could not close file "
                                  << filename.MaybeAsASCII();
      }
      return false;
    }
  }

  have_open_files_ = true;
  if (create) {
    out_entry_stat->last_modified = out_entry_stat->last_used = Time::Now();
    for (int i = 0; i < kSimpleEntryFileCount; ++i)
      out_entry_stat->data_size[i] = 0;
  } else {
    for (int i = 0; i < kSimpleEntryFileCount; ++i) {
      PlatformFileInfo file_info;
      bool success = GetPlatformFileInfo(files_[i], &file_info);
      base::Time file_last_modified;
      if (!success) {
        DLOG(WARNING) << "Could not get platform file info.";
        continue;
      }
      out_entry_stat->last_used = file_info.last_accessed;
      if (simple_util::GetMTime(path_, &file_last_modified))
        out_entry_stat->last_modified = file_last_modified;
      else
        out_entry_stat->last_modified = file_info.last_modified;

      // Keep the file size in |data size_| briefly until the key is initialized
      // properly.
      out_entry_stat->data_size[i] = file_info.size;
    }
  }

  return true;
}

void SimpleSynchronousEntry::CloseFiles() {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    DCHECK_NE(kInvalidPlatformFileValue, files_[i]);
    bool did_close = ClosePlatformFile(files_[i]);
    DCHECK(did_close);
  }
}

int SimpleSynchronousEntry::InitializeForOpen(bool had_index,
                                              SimpleEntryStat* out_entry_stat) {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(false, had_index, out_entry_stat))
    return net::ERR_FAILED;

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    SimpleFileHeader header;
    int header_read_result =
        ReadPlatformFile(files_[i], 0, reinterpret_cast<char*>(&header),
                         sizeof(header));
    if (header_read_result != sizeof(header)) {
      DLOG(WARNING) << "Cannot read header from entry.";
      RecordSyncOpenResult(OPEN_ENTRY_CANT_READ_HEADER, had_index);
      return net::ERR_FAILED;
    }

    if (header.initial_magic_number != kSimpleInitialMagicNumber) {
      // TODO(gavinp): This seems very bad; for now we log at WARNING, but we
      // should give consideration to not saturating the log with these if that
      // becomes a problem.
      DLOG(WARNING) << "Magic number did not match.";
      RecordSyncOpenResult(OPEN_ENTRY_BAD_MAGIC_NUMBER, had_index);
      return net::ERR_FAILED;
    }

    if (header.version != kSimpleVersion) {
      DLOG(WARNING) << "Unreadable version.";
      RecordSyncOpenResult(OPEN_ENTRY_BAD_VERSION, had_index);
      return net::ERR_FAILED;
    }

    scoped_ptr<char[]> key(new char[header.key_length]);
    int key_read_result = ReadPlatformFile(files_[i], sizeof(header),
                                           key.get(), header.key_length);
    if (key_read_result != implicit_cast<int>(header.key_length)) {
      DLOG(WARNING) << "Cannot read key from entry.";
      RecordSyncOpenResult(OPEN_ENTRY_CANT_READ_KEY, had_index);
      return net::ERR_FAILED;
    }

    key_ = std::string(key.get(), header.key_length);
    out_entry_stat->data_size[i] =
        GetDataSizeFromKeyAndFileSize(key_, out_entry_stat->data_size[i]);
    if (out_entry_stat->data_size[i] < 0) {
      // This entry can't possibly be valid, as it does not have enough space to
      // store a valid SimpleFileEOF record.
      return net::ERR_FAILED;
    }

    if (base::Hash(key.get(), header.key_length) != header.key_hash) {
      DLOG(WARNING) << "Hash mismatch on key.";
      RecordSyncOpenResult(OPEN_ENTRY_KEY_HASH_MISMATCH, had_index);
      return net::ERR_FAILED;
    }
  }
  RecordSyncOpenResult(OPEN_ENTRY_SUCCESS, had_index);
  initialized_ = true;
  return net::OK;
}

int SimpleSynchronousEntry::InitializeForCreate(
    bool had_index,
    SimpleEntryStat* out_entry_stat) {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(true, had_index, out_entry_stat)) {
    DLOG(WARNING) << "Could not create platform files.";
    return net::ERR_FILE_EXISTS;
  }
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    SimpleFileHeader header;
    header.initial_magic_number = kSimpleInitialMagicNumber;
    header.version = kSimpleVersion;

    header.key_length = key_.size();
    header.key_hash = base::Hash(key_);

    if (WritePlatformFile(files_[i], 0, reinterpret_cast<char*>(&header),
                          sizeof(header)) != sizeof(header)) {
      DLOG(WARNING) << "Could not write headers to new cache entry.";
      RecordSyncCreateResult(CREATE_ENTRY_CANT_WRITE_HEADER, had_index);
      return net::ERR_FAILED;
    }

    if (WritePlatformFile(files_[i], sizeof(header), key_.data(),
                          key_.size()) != implicit_cast<int>(key_.size())) {
      DLOG(WARNING) << "Could not write keys to new cache entry.";
      RecordSyncCreateResult(CREATE_ENTRY_CANT_WRITE_KEY, had_index);
      return net::ERR_FAILED;
    }
  }
  RecordSyncCreateResult(CREATE_ENTRY_SUCCESS, had_index);
  initialized_ = true;
  return net::OK;
}

void SimpleSynchronousEntry::Doom() const {
  // TODO(gavinp): Consider if we should guard against redundant Doom() calls.
  DeleteFilesForEntryHash(path_, entry_hash_);
}

}  // namespace disk_cache
