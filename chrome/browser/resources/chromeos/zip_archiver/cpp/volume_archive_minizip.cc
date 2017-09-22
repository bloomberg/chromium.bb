// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "volume_archive_minizip.h"

#include <time.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "ppapi/cpp/logging.h"

namespace {

base::Time::Exploded ExplodeDosdate(uint32_t dos_timedate) {
  base::Time::Exploded exploded_time = {};
  exploded_time.year = 1980 + ((dos_timedate >> 25) & 0x7f);
  exploded_time.month = (dos_timedate >> 21) & 0x0f;
  exploded_time.day_of_month = (dos_timedate >> 16) & 0x1f;
  exploded_time.hour = (dos_timedate >> 11) & 0x1f;
  exploded_time.minute = (dos_timedate >> 5) & 0x3f;
  exploded_time.second = (dos_timedate & 0x1f) << 1;
  exploded_time.millisecond = 0;

  return exploded_time;
}

};  // namespace

namespace volume_archive_functions {
void* CustomArchiveOpen(void* archive,
                        const char* /* filename */,
                        int /* mode */) {
  return archive;
}

int64_t DynamicCache(VolumeArchiveMinizip* archive, int64_t unzip_size) {
  int64_t offset = archive->reader()->offset();
  if (archive->reader()->Seek(static_cast<int64_t>(offset),
                              ZLIB_FILEFUNC_SEEK_SET) < 0) {
    return -1 /* Error */;
  }

  int64_t bytes_to_read =
      std::min(volume_archive_constants::kMaximumDataChunkSize,
               archive->reader()->archive_size() - offset);
  PP_DCHECK(bytes_to_read > 0);
  int64_t left_length = bytes_to_read;
  char* buffer_pointer = archive->dynamic_cache_;
  const void* destination_buffer;

  do {
    int64_t read_bytes =
        archive->reader()->Read(left_length, &destination_buffer);
    // End of the zip file.
    if (read_bytes == 0)
      break;
    if (read_bytes < 0)
      return -1 /* Error */;
    memcpy(buffer_pointer, destination_buffer, read_bytes);
    left_length -= read_bytes;
    buffer_pointer += read_bytes;
  } while (left_length > 0);

  if (archive->reader()->Seek(static_cast<int64_t>(offset),
                              ZLIB_FILEFUNC_SEEK_SET) < 0) {
    return -1 /* Error */;
  }
  archive->dynamic_cache_size_ = bytes_to_read - left_length;
  archive->dynamic_cache_offset_ = offset;

  return unzip_size - left_length;
}

uint32_t CustomArchiveRead(void* archive,
                           void* /* stream */,
                           void* buffer,
                           uint32_t size) {
  VolumeArchiveMinizip* archive_minizip =
      static_cast<VolumeArchiveMinizip*>(archive);
  int64_t offset = archive_minizip->reader()->offset();

  // When minizip requests a chunk in static_cache_.
  if (offset >= archive_minizip->static_cache_offset_) {
    // Relative offset in the central directory.
    int64_t offset_in_cache = offset - archive_minizip->static_cache_offset_;
    memcpy(buffer, archive_minizip->static_cache_ + offset_in_cache, size);
    if (archive_minizip->reader()->Seek(static_cast<int64_t>(size),
                                        ZLIB_FILEFUNC_SEEK_CUR) < 0) {
      return -1 /* Error */;
    }
    return size;
  }

  char* unzip_buffer_pointer = static_cast<char*>(buffer);
  int64_t left_length = static_cast<int64_t>(size);

  do {
    offset = archive_minizip->reader()->offset();
    // If dynamic_cache_ is empty or it cannot be reused, update the cache so
    // that it contains the chunk required by minizip.
    if (archive_minizip->dynamic_cache_size_ == 0 ||
        offset < archive_minizip->dynamic_cache_offset_ ||
        archive_minizip->dynamic_cache_offset_ +
                archive_minizip->dynamic_cache_size_ <
            offset + size) {
      volume_archive_functions::DynamicCache(archive_minizip, size);
    }

    // Just copy the required data from the cache.
    int64_t offset_in_cache = offset - archive_minizip->dynamic_cache_offset_;
    int64_t copy_length = std::min(
        left_length, archive_minizip->dynamic_cache_size_ - offset_in_cache);
    memcpy(unzip_buffer_pointer,
           archive_minizip->dynamic_cache_ + offset_in_cache, copy_length);
    unzip_buffer_pointer += copy_length;
    left_length -= copy_length;
    if (archive_minizip->reader()->Seek(static_cast<int64_t>(copy_length),
                                        ZLIB_FILEFUNC_SEEK_CUR) < 0) {
      return -1 /* Error */;
    }
  } while (left_length > 0);

  return size;
}

uint32_t CustomArchiveWrite(void* /*archive*/,
                            void* /*stream*/,
                            const void* /*buffer*/,
                            uint32_t /*length*/) {
  return 0 /* Success */;
}

long CustomArchiveTell(void* archive, void* /*stream*/) {
  VolumeArchiveMinizip* archive_minizip =
      static_cast<VolumeArchiveMinizip*>(archive);
  return static_cast<long>(archive_minizip->reader()->offset());
}

long CustomArchiveSeek(void* archive,
                       void* /*stream*/,
                       uint32_t offset,
                       int origin) {
  VolumeArchiveMinizip* archive_minizip =
      static_cast<VolumeArchiveMinizip*>(archive);

  long return_value = static_cast<long>(archive_minizip->reader()->Seek(
      static_cast<int64_t>(offset), static_cast<int64_t>(origin)));
  if (return_value >= 0)
    return 0 /* Success */;
  return -1 /* Error */;
}

int CustomArchiveClose(void* /*opaque*/, void* /*stream*/) {
  return 0;
}

int CustomArchiveError(void* /*opaque*/, void* /*stream*/) {
  return 0;
}

std::unique_ptr<std::string> GetPassphrase(
    VolumeArchiveMinizip* archive_minizip) {
  return archive_minizip->reader()->Passphrase();
}

}  // namespace volume_archive_functions

VolumeArchiveMinizip::VolumeArchiveMinizip(VolumeReader* reader)
    : VolumeArchive(reader),
      reader_data_size_(volume_archive_constants::kMinimumDataChunkSize),
      zip_file_(nullptr),
      dynamic_cache_offset_(0),
      dynamic_cache_size_(0),
      static_cache_offset_(0),
      static_cache_size_(0),
      last_read_data_offset_(0),
      last_read_data_length_(0),
      decompressed_data_(nullptr),
      decompressed_data_size_(0),
      decompressed_error_(false) {}

VolumeArchiveMinizip::~VolumeArchiveMinizip() {
  Cleanup();
}

bool VolumeArchiveMinizip::Init(const std::string& encoding) {
  // Set up minizip object.
  zlib_filefunc_def zip_funcs;
  zip_funcs.zopen_file = volume_archive_functions::CustomArchiveOpen;
  zip_funcs.zread_file = volume_archive_functions::CustomArchiveRead;
  zip_funcs.zwrite_file = volume_archive_functions::CustomArchiveWrite;
  zip_funcs.ztell_file = volume_archive_functions::CustomArchiveTell;
  zip_funcs.zseek_file = volume_archive_functions::CustomArchiveSeek;
  zip_funcs.zclose_file = volume_archive_functions::CustomArchiveClose;
  zip_funcs.zerror_file = volume_archive_functions::CustomArchiveError;
  zip_funcs.opaque = static_cast<void*>(this);

  // Load maximum static_cache_size_ bytes from the end of the archive to
  // static_cache_.
  static_cache_size_ = std::min(volume_archive_constants::kStaticCacheSize,
                                reader()->archive_size());
  int64_t previous_offset = reader()->offset();
  char* buffer_pointer = static_cache_;
  int64_t left_length = static_cache_size_;
  static_cache_offset_ =
      std::max(reader()->archive_size() - static_cache_size_, 0LL);
  if (reader()->Seek(static_cache_offset_, ZLIB_FILEFUNC_SEEK_SET) < 0) {
    set_error_message(volume_archive_constants::kArchiveOpenError);
    return false /* Error */;
  }
  do {
    const void* destination_buffer;
    int64_t read_bytes = reader()->Read(left_length, &destination_buffer);
    memcpy(buffer_pointer, destination_buffer, read_bytes);
    left_length -= read_bytes;
    buffer_pointer += read_bytes;
  } while (left_length > 0);

  // Set the offset to the original position.
  if (reader()->Seek(previous_offset, ZLIB_FILEFUNC_SEEK_SET) < 0) {
    set_error_message(volume_archive_constants::kArchiveOpenError);
    return false /* Error */;
  }

  zip_file_ = unzOpen2(nullptr /* filename */, &zip_funcs);
  if (!zip_file_) {
    set_error_message(volume_archive_constants::kArchiveOpenError);
    return false;
  }

  return true;
}

VolumeArchive::Result VolumeArchiveMinizip::GetCurrentFileInfo(
    std::string* pathname,
    int64_t* size,
    bool* is_directory,
    time_t* modification_time) {
  // Headers are being read from the central directory (in the ZIP format), so
  // use a large block size to save on IPC calls. The headers in EOCD are
  // grouped one by one.
  reader_data_size_ = volume_archive_constants::kMaximumDataChunkSize;

  // Reset to 0 for new VolumeArchive::ReadData operation.
  last_read_data_offset_ = 0;
  decompressed_data_size_ = 0;

  unz_file_pos position = {};
  if (unzGetFilePos(zip_file_, &position) != UNZ_OK) {
    set_error_message(volume_archive_constants::kArchiveNextHeaderError);
    return VolumeArchive::RESULT_FAIL;
  }

  // Get the information of the opened file.
  unz_file_info raw_file_info = {};
  char raw_file_name_in_zip[volume_archive_constants::kZipMaxPath] = {};
  const int result =
      unzGetCurrentFileInfo(zip_file_, &raw_file_info, raw_file_name_in_zip,
                            sizeof(raw_file_name_in_zip) - 1,
                            nullptr,  // extraField.
                            0,        // extraFieldBufferSize.
                            nullptr,  // szComment.
                            0);       // commentBufferSize.

  if (result != UNZ_OK || raw_file_name_in_zip[0] == '\0') {
    set_error_message(volume_archive_constants::kArchiveNextHeaderError);
    return VolumeArchive::RESULT_FAIL;
  }

  *pathname = std::string(raw_file_name_in_zip);
  *size = raw_file_info.uncompressed_size;
  // Directory entries in zip files end with "/".
  *is_directory = base::EndsWith(raw_file_name_in_zip, "/",
                                 base::CompareCase::INSENSITIVE_ASCII);

  // Construct the last modified time. The timezone info is not present in
  // zip files. By default, the time is set as local time in zip format.
  base::Time local_time;
  // If the modification time is not available, we set the value to the current
  // local time.
  if (!base::Time::FromLocalExploded(ExplodeDosdate(raw_file_info.dos_date),
                                     &local_time))
    local_time = base::Time::UnixEpoch();
  *modification_time = local_time.ToTimeT();

  return VolumeArchive::RESULT_SUCCESS;
}

VolumeArchive::Result VolumeArchiveMinizip::GoToNextFile() {
  int return_value = unzGoToNextFile(zip_file_);
  if (return_value == UNZ_END_OF_LIST_OF_FILE) {
    return VolumeArchive::RESULT_EOF;
  }
  if (return_value == UNZ_OK)
    return VolumeArchive::RESULT_SUCCESS;

  set_error_message(volume_archive_constants::kArchiveNextHeaderError);
  return VolumeArchive::RESULT_FAIL;
}

bool VolumeArchiveMinizip::SeekHeader(const std::string& path_name) {
  // Reset to 0 for new VolumeArchive::ReadData operation.
  last_read_data_offset_ = 0;
  decompressed_data_size_ = 0;

  // Setting nullptr to filename_compare_func falls back to strcmp, i.e. case
  // sensitive.
  if (unzLocateFile(zip_file_, path_name.c_str(),
                    nullptr /* filename_compare_func */) != UNZ_OK) {
    set_error_message(volume_archive_constants::kArchiveNextHeaderError);
    return false;
  }

  unz_file_info raw_file_info = {};
  char raw_file_name_in_zip[volume_archive_constants::kZipMaxPath] = {};
  if (unzGetCurrentFileInfo(zip_file_, &raw_file_info, raw_file_name_in_zip,
                            sizeof(raw_file_name_in_zip) - 1,
                            nullptr,  // extraField.
                            0,        // extraFieldBufferSize.
                            nullptr,  // szComment.
                            0) != UNZ_OK) {
    set_error_message(volume_archive_constants::kArchiveNextHeaderError);
    return false;
  }

  // Directory entries in zip files end with "/".
  bool is_directory = base::EndsWith(raw_file_name_in_zip, "/",
                                     base::CompareCase::INSENSITIVE_ASCII);

  int open_result = UNZ_OK;
  // If the archive is encrypted, the lowest bit of raw_file_info.flag is set.
  // Directories cannot be encrypted with the basic zip encrytion algorithm.
  if (((raw_file_info.flag & 1) != 0) && !is_directory) {
    do {
      if (password_cache_ == nullptr) {
        // Save passphrase for upcoming file requests.
        password_cache_ = volume_archive_functions::GetPassphrase(this);
        // check if |password_cache_| is nullptr in case when user clicks Cancel
        if (password_cache_ == nullptr) {
          return false;
        }
      }

      open_result =
          unzOpenCurrentFilePassword(zip_file_, password_cache_.get()->c_str());

      // If password is incorrect then password cache ought to be reseted.
      if (open_result == UNZ_BADPASSWORD && password_cache_ != nullptr)
        password_cache_.reset();

    } while (open_result == UNZ_BADPASSWORD);
  } else {
    open_result = unzOpenCurrentFile(zip_file_);
  }

  if (open_result != UNZ_OK) {
    set_error_message(volume_archive_constants::kArchiveNextHeaderError);
    return false;
  }

  return true;
}

void VolumeArchiveMinizip::DecompressData(int64_t offset, int64_t length) {
  // TODO(cmihail): As an optimization consider using archive_read_data_block
  // which avoids extra copying in case offset != last_read_data_offset_.
  // The logic will be more complicated because archive_read_data_block offset
  // will not be aligned with the offset of the read request from JavaScript.

  // Requests with offset smaller than last read offset are not supported.
  if (offset < last_read_data_offset_) {
    set_error_message(
        std::string(volume_archive_constants::kArchiveReadDataError));
    decompressed_error_ = true;
    return;
  }

  // Request with offset greater than last read offset. Skip not needed bytes.
  // Because files are compressed, seeking is not possible, so all of the bytes
  // until the requested position must be unpacked.
  ssize_t size = -1;
  while (offset > last_read_data_offset_) {
    // ReadData will call CustomArchiveRead when calling archive_read_data. Read
    // should not request more bytes than possibly needed, so we request either
    // offset - last_read_data_offset_, kMaximumDataChunkSize in case the former
    // is too big or kMinimumDataChunkSize in case its too small and we might
    // end up with too many IPCs.
    reader_data_size_ =
        std::max(std::min(offset - last_read_data_offset_,
                          volume_archive_constants::kMaximumDataChunkSize),
                 volume_archive_constants::kMinimumDataChunkSize);

    // No need for an offset in dummy_buffer as it will be ignored anyway.
    // archive_read_data receives size_t as length parameter, but we limit it to
    // volume_archive_constants::kDummyBufferSize which is positive and less
    // than size_t maximum. So conversion from int64_t to size_t is safe here.
    size = unzReadCurrentFile(
        zip_file_, dummy_buffer_,
        std::min(offset - last_read_data_offset_,
                 volume_archive_constants::kDummyBufferSize));
    PP_DCHECK(size != 0);  // The actual read is done below. We shouldn't get to
                           // end of file here.
    if (size < 0) {        // Error.
      set_error_message(volume_archive_constants::kArchiveReadDataError);
      decompressed_error_ = true;
      return;
    }
    last_read_data_offset_ += size;
  }

  // Do not decompress more bytes than we can store internally. The
  // kDecompressBufferSize limit is used to avoid huge memory usage.
  int64_t left_length =
      std::min(length, volume_archive_constants::kDecompressBufferSize);

  // ReadData will call CustomArchiveRead when calling archive_read_data. The
  // read should be done with a value similar to length, which is the requested
  // number of bytes, or kMaximumDataChunkSize / kMinimumDataChunkSize
  // in case length is too big or too small.
  reader_data_size_ =
      std::max(std::min(static_cast<int64_t>(left_length),
                        volume_archive_constants::kMaximumDataChunkSize),
               volume_archive_constants::kMinimumDataChunkSize);

  // Perform the actual copy.
  int64_t bytes_read = 0;
  do {
    // archive_read_data receives size_t as length parameter, but we limit it to
    // volume_archive_constants::kMinimumDataChunkSize (see left_length
    // initialization), which is positive and less than size_t maximum.
    // So conversion from int64_t to size_t is safe here.
    size = unzReadCurrentFile(zip_file_, decompressed_data_buffer_ + bytes_read,
                              left_length);
    if (size < 0) {  // Error.
      set_error_message(volume_archive_constants::kArchiveReadDataError);
      decompressed_error_ = true;
      return;
    }
    bytes_read += size;
    left_length -= size;
  } while (left_length > 0 && size != 0);  // There is still data to read.

  // VolumeArchiveMinizip::DecompressData always stores the data from
  // beginning of the buffer. VolumeArchiveMinizip::ConsumeData is used
  // to preserve the bytes that are decompressed but not required by
  // VolumeArchiveMinizip::ReadData.
  decompressed_data_ = decompressed_data_buffer_;
  decompressed_data_size_ = bytes_read;
}

bool VolumeArchiveMinizip::Cleanup() {
  bool returnValue = true;
  if (zip_file_) {
    if (unzClose(zip_file_) != UNZ_OK) {
      set_error_message(volume_archive_constants::kArchiveReadFreeError);
      returnValue = false;
    }
  }
  zip_file_ = nullptr;
  password_cache_.reset();

  CleanupReader();

  return returnValue;
}

int64_t VolumeArchiveMinizip::ReadData(int64_t offset,
                                       int64_t length,
                                       const char** buffer) {
  PP_DCHECK(length > 0);  // Length must be at least 1.
  // In case of first read or no more available data in the internal buffer or
  // offset is different from the last_read_data_offset_, then force
  // VolumeArchiveMinizip::DecompressData as the decompressed data is
  // invalid.
  if (!decompressed_data_ || last_read_data_offset_ != offset ||
      decompressed_data_size_ == 0)
    DecompressData(offset, length);

  // Decompressed failed.
  if (decompressed_error_) {
    set_error_message(volume_archive_constants::kArchiveReadDataError);
    return -1 /* Error */;
  }

  last_read_data_length_ = length;  // Used for decompress ahead.

  // Assign the output *buffer parameter to the internal buffer.
  *buffer = decompressed_data_;

  // Advance internal buffer for next ReadData call.
  int64_t read_bytes = std::min(decompressed_data_size_, length);
  decompressed_data_ = decompressed_data_ + read_bytes;
  decompressed_data_size_ -= read_bytes;
  last_read_data_offset_ += read_bytes;

  PP_DCHECK(decompressed_data_ + decompressed_data_size_ <=
            decompressed_data_buffer_ +
                volume_archive_constants::kDecompressBufferSize);

  return read_bytes;
}

void VolumeArchiveMinizip::MaybeDecompressAhead() {
  if (decompressed_data_size_ == 0)
    DecompressData(last_read_data_offset_, last_read_data_length_);
}
