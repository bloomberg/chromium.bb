// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DISK_CACHE_ENTRY_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_DISK_CACHE_ENTRY_ELEMENT_READER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/upload_element_reader.h"

namespace disk_cache {
class Entry;
}

namespace net {

// An UploadElementReader implementation for disk_cache::Entry objects. The
// caller keeps ownership of |disk_cache_entry|, and is responsible for ensuring
// it outlives the UploadDiskCacheEntryElementReader.
class NET_EXPORT UploadDiskCacheEntryElementReader
    : public UploadElementReader {
 public:
  // Construct a new UploadDiskCacheEntryElementReader which reads from the disk
  // cache entry |disk_cache_entry| with stream index |disk_cache_stream_index|.
  // The new upload reader object will read |range_length| bytes, starting from
  // |range_offset|. To read an whole cache entry give a 0 as |range_offset| and
  // provide the length of the entry's stream as |range_length|.
  UploadDiskCacheEntryElementReader(disk_cache::Entry* disk_cache_entry,
                                    int disk_cache_stream_index,
                                    int range_offset,
                                    int range_length);
  ~UploadDiskCacheEntryElementReader() override;

  int range_offset_for_tests() const { return range_begin_offset_; }
  int range_length_for_tests() const {
    return range_end_offset_ - range_begin_offset_;
  }

  // UploadElementReader overrides:
  const UploadDiskCacheEntryElementReader* AsDiskCacheEntryReaderForTests()
      const override;
  int Init(const CompletionCallback& callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  bool IsInMemory() const override;
  int Read(IOBuffer* buf,
           int buf_length,
           const CompletionCallback& callback) override;

 private:
  void OnReadCompleted(const CompletionCallback& callback, int result);

  disk_cache::Entry* const disk_cache_entry_;
  const int disk_cache_stream_index_;

  const int range_begin_offset_;
  const int range_end_offset_;

  int current_read_offset_;

  base::WeakPtrFactory<UploadDiskCacheEntryElementReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UploadDiskCacheEntryElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DISK_CACHE_ENTRY_ELEMENT_READER_H_
