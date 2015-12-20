// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_
#define NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/upload_data_stream.h"

namespace net {

class IOBuffer;

// Class with a push-based interface for uploading data. Buffers all data until
// the request is completed. Not recommended for uploading large amounts of
// seekable data, due to this buffering behavior.
class NET_EXPORT ChunkedUploadDataStream : public UploadDataStream {
 public:
  explicit ChunkedUploadDataStream(int64_t identifier);

  ~ChunkedUploadDataStream() override;

  // Adds data to the stream. |is_done| should be true if this is the last
  // data to be appended. |data_len| must not be 0 unless |is_done| is true.
  // Once called with |is_done| being true, must never be called again.
  // TODO(mmenke):  Consider using IOBuffers instead, to reduce data copies.
  void AppendData(const char* data, int data_len, bool is_done);

 private:
  // UploadDataStream implementation.
  int InitInternal() override;
  int ReadInternal(IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  int ReadChunk(IOBuffer* buf, int buf_len);

  // Index and offset of next element of |upload_data_| to be read.
  size_t read_index_;
  size_t read_offset_;

  // True once all data has been appended to the stream.
  bool all_data_appended_;

  std::vector<scoped_ptr<std::vector<char>>> upload_data_;

  // Buffer to write the next read's data to. Only set when a call to
  // ReadInternal reads no data.
  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_len_;

  DISALLOW_COPY_AND_ASSIGN(ChunkedUploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_
