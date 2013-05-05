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
#include "base/stringprintf.h"
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
  OPEN_ENTRY_KEY_MISMATCH = 6,
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

void RecordSyncOpenResult(OpenEntryResult result) {
  DCHECK_GT(OPEN_ENTRY_MAX, result);
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncOpenResult", result, OPEN_ENTRY_MAX);
}

void RecordSyncCreateResult(CreateEntryResult result) {
  DCHECK_GT(CREATE_ENTRY_MAX, result);
  UMA_HISTOGRAM_ENUMERATION(
      "SimpleCache.SyncCreateResult", result, CREATE_ENTRY_MAX);
}

}

namespace disk_cache {

using simple_util::ConvertEntryHashKeyToHexString;
using simple_util::GetEntryHashKey;
using simple_util::GetFilenameFromEntryHashAndIndex;
using simple_util::GetDataSizeFromKeyAndFileSize;
using simple_util::GetFileSizeFromKeyAndDataSize;
using simple_util::GetFileOffsetFromKeyAndDataOffset;

SimpleSynchronousEntry::CRCRecord::CRCRecord() : index(-1),
                                                 has_crc32(false),
                                                 data_crc32(0) {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord(int index_p,
                                             bool has_crc32_p,
                                             uint32 data_crc32_p)
  : index(index_p),
    has_crc32(has_crc32_p),
    data_crc32(data_crc32_p) {
}

// static
void SimpleSynchronousEntry::OpenEntry(
    const FilePath& path,
    const std::string& key,
    const uint64 entry_hash,
    SimpleSynchronousEntry** out_entry,
    int* out_result) {
  DCHECK_EQ(entry_hash, simple_util::GetEntryHashKey(key));
  SimpleSynchronousEntry* sync_entry = new SimpleSynchronousEntry(path, key,
                                                                  entry_hash);
  *out_result = sync_entry->InitializeForOpen();
  if (*out_result != net::OK) {
    sync_entry->Doom();
    delete sync_entry;
    *out_entry = NULL;
    return;
  }
  *out_entry = sync_entry;
}

// static
void SimpleSynchronousEntry::CreateEntry(
    const FilePath& path,
    const std::string& key,
    const uint64 entry_hash,
    SimpleSynchronousEntry** out_entry,
    int* out_result) {
  DCHECK_EQ(entry_hash, GetEntryHashKey(key));
  SimpleSynchronousEntry* sync_entry = new SimpleSynchronousEntry(path, key,
                                                                  entry_hash);
  *out_result = sync_entry->InitializeForCreate();
  if (*out_result != net::OK) {
    if (*out_result != net::ERR_FILE_EXISTS)
      sync_entry->Doom();
    delete sync_entry;
    return;
  }
  *out_entry = sync_entry;
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
    if (!file_util::Delete(to_delete, false)) {
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
void SimpleSynchronousEntry::DoomEntrySet(
    scoped_ptr<std::vector<uint64> > key_hashes,
    const FilePath& path,
    int* out_result) {
  const size_t did_delete_count = std::count_if(
      key_hashes->begin(), key_hashes->end(), std::bind1st(
          std::ptr_fun(SimpleSynchronousEntry::DeleteFilesForEntryHash), path));
  *out_result = (did_delete_count == key_hashes->size()) ? net::OK
                                                         : net::ERR_FAILED;
}

void SimpleSynchronousEntry::ReadData(
    int index,
    int offset,
    net::IOBuffer* buf,
    int buf_len,
    uint32* out_crc32,
    int* out_result) {
  DCHECK(initialized_);
  int64 file_offset = GetFileOffsetFromKeyAndDataOffset(key_, offset);
  int bytes_read = ReadPlatformFile(files_[index], file_offset,
                                    buf->data(), buf_len);
  if (bytes_read > 0) {
    last_used_ = Time::Now();
    *out_crc32 = crc32(crc32(0L, Z_NULL, 0),
                       reinterpret_cast<const Bytef*>(buf->data()), bytes_read);
  }
  if (bytes_read >= 0) {
    *out_result = bytes_read;
  } else {
    *out_result = net::ERR_FAILED;
    Doom();
  }
}

void SimpleSynchronousEntry::WriteData(
    int index,
    int offset,
    net::IOBuffer* buf,
    int buf_len,
    bool truncate,
    int* out_result) {
  DCHECK(initialized_);

  bool extending_by_write = offset + buf_len > data_size_[index];
  if (extending_by_write) {
    // We are extending the file, and need to insure the EOF record is zeroed.
    const int64 file_eof_offset =
        GetFileOffsetFromKeyAndDataOffset(key_, data_size_[index]);
    if (!TruncatePlatformFile(files_[index], file_eof_offset)) {
      Doom();
      *out_result = net::ERR_FAILED;
      return;
    }
  }
  const int64 file_offset = GetFileOffsetFromKeyAndDataOffset(key_, offset);
  if (buf_len > 0) {
    if (WritePlatformFile(files_[index], file_offset, buf->data(), buf_len) !=
        buf_len) {
      Doom();
      *out_result = net::ERR_FAILED;
      return;
    }
  }
  if (!truncate && (buf_len > 0 || !extending_by_write)) {
    data_size_[index] = std::max(data_size_[index], offset + buf_len);
  } else {
    if (!TruncatePlatformFile(files_[index], file_offset + buf_len)) {
      Doom();
      *out_result = net::ERR_FAILED;
      return;
    }
    data_size_[index] = offset + buf_len;
  }

  last_used_ = last_modified_ = Time::Now();
  *out_result = buf_len;
}

void SimpleSynchronousEntry::CheckEOFRecord(
    int index,
    uint32 expected_crc32,
    int* out_result) {
  DCHECK(initialized_);

  SimpleFileEOF eof_record;
  int64 file_offset = GetFileOffsetFromKeyAndDataOffset(key_,
                                                        data_size_[index]);
  if (ReadPlatformFile(files_[index], file_offset,
                       reinterpret_cast<char*>(&eof_record),
                       sizeof(eof_record)) != sizeof(eof_record)) {
    Doom();
    *out_result = net::ERR_FAILED;
    return;
  }

  if (eof_record.final_magic_number != kSimpleFinalMagicNumber) {
    DLOG(INFO) << "eof record had bad magic number.";
    Doom();
    *out_result = net::ERR_FAILED;
    return;
  }

  if ((eof_record.flags & SimpleFileEOF::FLAG_HAS_CRC32) &&
      eof_record.data_crc32 != expected_crc32) {
    DLOG(INFO) << "eof record had bad crc.";
    Doom();
    *out_result = net::ERR_FAILED;
    return;
  }

  *out_result = net::OK;
}

void SimpleSynchronousEntry::Close(
    scoped_ptr<std::vector<CRCRecord> > crc32s_to_write) {
  for (std::vector<CRCRecord>::const_iterator it = crc32s_to_write->begin();
       it != crc32s_to_write->end(); ++it) {
    SimpleFileEOF eof_record;
    eof_record.final_magic_number = kSimpleFinalMagicNumber;
    eof_record.flags = 0;
    if (it->has_crc32)
      eof_record.flags |= SimpleFileEOF::FLAG_HAS_CRC32;
    eof_record.data_crc32 = it->data_crc32;
    int64 file_offset =
        GetFileOffsetFromKeyAndDataOffset(key_, data_size_[it->index]);
    if (WritePlatformFile(files_[it->index], file_offset,
                          reinterpret_cast<const char*>(&eof_record),
                          sizeof(eof_record)) != sizeof(eof_record)) {
      DLOG(INFO) << "Could not write eof record.";
      Doom();
      break;
    }
  }

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    bool did_close_file = ClosePlatformFile(files_[i]);
    CHECK(did_close_file);
  }
  have_open_files_ = false;
  delete this;
}

int64 SimpleSynchronousEntry::GetFileSize() const {
  int64 file_size = 0;
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    file_size += GetFileSizeFromKeyAndDataSize(key_, data_size_[i]);
  }
  return file_size;
}

SimpleSynchronousEntry::SimpleSynchronousEntry(
    const FilePath& path,
    const std::string& key,
    const uint64 entry_hash)
    : path_(path),
      key_(key),
      entry_hash_(entry_hash),
      have_open_files_(false),
      initialized_(false) {
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(files_),
                 array_sizes_must_be_equal);
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    files_[i] = kInvalidPlatformFileValue;
  }
}

SimpleSynchronousEntry::~SimpleSynchronousEntry() {
  DCHECK(!(have_open_files_ && initialized_));
  if (have_open_files_)
    CloseFiles();
}

bool SimpleSynchronousEntry::OpenOrCreateFiles(bool create) {
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
      if (create) {
        RecordSyncCreateResult(CREATE_ENTRY_PLATFORM_FILE_ERROR);
        UMA_HISTOGRAM_ENUMERATION("SimpleCache.SyncCreatePlatformFileError",
                                  -error, -base::PLATFORM_FILE_ERROR_MAX);
      } else {
        RecordSyncOpenResult(OPEN_ENTRY_PLATFORM_FILE_ERROR);
        UMA_HISTOGRAM_ENUMERATION("SimpleCache.SyncOpenPlatformFileError",
                                  -error, -base::PLATFORM_FILE_ERROR_MAX);
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
    last_modified_ = last_used_ = Time::Now();
    for (int i = 0; i < kSimpleEntryFileCount; ++i)
      data_size_[i] = 0;
  } else {
    for (int i = 0; i < kSimpleEntryFileCount; ++i) {
      PlatformFileInfo file_info;
      bool success = GetPlatformFileInfo(files_[i], &file_info);
      if (!success) {
        DLOG(WARNING) << "Could not get platform file info.";
        continue;
      }
      last_used_ = std::max(last_used_, file_info.last_accessed);
      last_modified_ = std::max(last_modified_, file_info.last_modified);
      data_size_[i] = GetDataSizeFromKeyAndFileSize(key_, file_info.size);
      if (data_size_[i] < 0) {
        // This entry can't possibly be valid, as it does not enough space to
        // store a valid SimpleFileEOF record.
        return false;
      }
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

int SimpleSynchronousEntry::InitializeForOpen() {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(false))
    return net::ERR_FAILED;

  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    SimpleFileHeader header;
    int header_read_result =
        ReadPlatformFile(files_[i], 0, reinterpret_cast<char*>(&header),
                         sizeof(header));
    if (header_read_result != sizeof(header)) {
      DLOG(WARNING) << "Cannot read header from entry.";
      RecordSyncOpenResult(OPEN_ENTRY_CANT_READ_HEADER);
      return net::ERR_FAILED;
    }

    if (header.initial_magic_number != kSimpleInitialMagicNumber) {
      // TODO(gavinp): This seems very bad; for now we log at WARNING, but we
      // should give consideration to not saturating the log with these if that
      // becomes a problem.
      DLOG(WARNING) << "Magic number did not match.";
      RecordSyncOpenResult(OPEN_ENTRY_BAD_MAGIC_NUMBER);
      return net::ERR_FAILED;
    }

    if (header.version != kSimpleVersion) {
      DLOG(WARNING) << "Unreadable version.";
      RecordSyncOpenResult(OPEN_ENTRY_BAD_VERSION);
      return net::ERR_FAILED;
    }

    scoped_ptr<char[]> key(new char[header.key_length]);
    int key_read_result = ReadPlatformFile(files_[i], sizeof(header),
                                           key.get(), header.key_length);
    if (key_read_result != implicit_cast<int>(header.key_length)) {
      DLOG(WARNING) << "Cannot read key from entry.";
      RecordSyncOpenResult(OPEN_ENTRY_CANT_READ_KEY);
      return net::ERR_FAILED;
    }
    if (header.key_length != key_.size() ||
        std::memcmp(key_.data(), key.get(), key_.size()) != 0) {
      // TODO(gavinp): Since the way we use entry hash to name entries means
      // this is expected to occur at some frequency, add unit_tests that this
      // does is handled gracefully at higher levels.
      DLOG(WARNING) << "Key mismatch on open.";
      RecordSyncOpenResult(OPEN_ENTRY_KEY_MISMATCH);
      return net::ERR_FAILED;
    }

    if (base::Hash(key.get(), header.key_length) != header.key_hash) {
      DLOG(WARNING) << "Hash mismatch on key.";
      RecordSyncOpenResult(OPEN_ENTRY_KEY_HASH_MISMATCH);
      return net::ERR_FAILED;
    }
  }

  initialized_ = true;
  return net::OK;
}

int SimpleSynchronousEntry::InitializeForCreate() {
  DCHECK(!initialized_);
  if (!OpenOrCreateFiles(true)) {
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
      RecordSyncCreateResult(CREATE_ENTRY_CANT_WRITE_HEADER);
      return net::ERR_FAILED;
    }

    if (WritePlatformFile(files_[i], sizeof(header), key_.data(),
                          key_.size()) != implicit_cast<int>(key_.size())) {
      DLOG(WARNING) << "Could not write keys to new cache entry.";
      RecordSyncCreateResult(CREATE_ENTRY_CANT_WRITE_KEY);
      return net::ERR_FAILED;
    }
  }
  initialized_ = true;
  return net::OK;
}

void SimpleSynchronousEntry::Doom() {
  // TODO(gavinp): Consider if we should guard against redundant Doom() calls.
  DeleteFilesForEntryHash(path_, entry_hash_);
}

}  // namespace disk_cache
