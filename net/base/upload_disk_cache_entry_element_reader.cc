// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_disk_cache_entry_element_reader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

namespace net {

UploadDiskCacheEntryElementReader::UploadDiskCacheEntryElementReader(
    disk_cache::Entry* disk_cache_entry,
    int disk_cache_stream_index,
    int range_offset,
    int range_length)
    : disk_cache_entry_(disk_cache_entry),
      disk_cache_stream_index_(disk_cache_stream_index),
      range_begin_offset_(range_offset),
      range_end_offset_(range_offset + range_length),
      current_read_offset_(range_offset),
      weak_factory_(this) {
  DCHECK_LE(0, range_offset);
  DCHECK_LT(0, range_length);
  DCHECK_LE(range_offset + range_length,
            disk_cache_entry_->GetDataSize(disk_cache_stream_index_));
}

UploadDiskCacheEntryElementReader::~UploadDiskCacheEntryElementReader() {
}

const UploadDiskCacheEntryElementReader*
UploadDiskCacheEntryElementReader::AsDiskCacheEntryReaderForTests() const {
  return this;
}

int UploadDiskCacheEntryElementReader::Init(
    const CompletionCallback& callback) {
  weak_factory_.InvalidateWeakPtrs();
  current_read_offset_ = range_begin_offset_;
  return OK;
}

uint64_t UploadDiskCacheEntryElementReader::GetContentLength() const {
  return range_end_offset_ - range_begin_offset_;
}

uint64_t UploadDiskCacheEntryElementReader::BytesRemaining() const {
  return range_end_offset_ - current_read_offset_;
}

bool UploadDiskCacheEntryElementReader::IsInMemory() const {
  return false;
}

int UploadDiskCacheEntryElementReader::Read(
    IOBuffer* buf,
    int buf_length,
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  int bytes_to_read = std::min(buf_length, static_cast<int>(BytesRemaining()));

  CompletionCallback new_callback =
      base::Bind(&UploadDiskCacheEntryElementReader::OnReadCompleted,
                 weak_factory_.GetWeakPtr(), callback);

  int result = disk_cache_entry_->ReadData(disk_cache_stream_index_,
                                           current_read_offset_, buf,
                                           bytes_to_read, new_callback);
  if (result == ERR_IO_PENDING)
    return ERR_IO_PENDING;
  if (result > 0)
    current_read_offset_ += result;
  return result;
}

void UploadDiskCacheEntryElementReader::OnReadCompleted(
    const CompletionCallback& callback,
    int result) {
  if (result > 0)
    current_read_offset_ += result;
  callback.Run(result);
}

}  // namespace net
