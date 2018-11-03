// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_GZIP_SOURCE_STREAM_H_
#define NET_FILTER_GZIP_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/zlib_stream_wrapper.h"

namespace net {

class IOBuffer;

// GZipSourceStream applies gzip and deflate content encoding/decoding to a data
// stream. As specified by HTTP 1.1, with gzip encoding the content is
// wrapped with a gzip header, and with deflate encoding the content is in
// a raw, headerless DEFLATE stream.
//
// Internally GZipSourceStream uses zlib inflate to do decoding.
//
class NET_EXPORT_PRIVATE GzipSourceStream : public FilterSourceStream {
 public:
  ~GzipSourceStream() override;

  // Creates a GzipSourceStream. Return nullptr if initialization fails.
  static std::unique_ptr<GzipSourceStream> Create(
      std::unique_ptr<SourceStream> previous,
      SourceStream::SourceType type);

 private:
  GzipSourceStream(std::unique_ptr<SourceStream> previous,
                   SourceStream::SourceType type);

  // Returns true if initialization is successful, false otherwise.
  // For instance, this method returns false if there is not enough memory or
  // if there is a version mismatch.
  bool Init();

  // SourceStream implementation
  std::string GetTypeAsString() const override;
  int FilterData(IOBuffer* output_buffer,
                 int output_buffer_size,
                 IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_end_reached) override;

  ZLibStreamWrapper zlib_stream_;

  DISALLOW_COPY_AND_ASSIGN(GzipSourceStream);
};

}  // namespace net

#endif  // NET_FILTER_GZIP_SOURCE_STREAM_H__
