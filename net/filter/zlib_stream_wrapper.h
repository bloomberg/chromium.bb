// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_ZLIB_STREAM_WRAPPER_H_
#define NET_FILTER_ZLIB_STREAM_WRAPPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/filter/gzip_header.h"
#include "third_party/zlib/zlib.h"

namespace net {

class IOBuffer;

// Class to wrap and provide an easier to use interface to zlib for inflating
// zlib or gzip wrapped deflated data streams.
class NET_EXPORT ZLibStreamWrapper {
 public:
  // The stream source type to be inflated.
  enum class SourceType {
    // Source stream is a gzip-wrapped deflate data.
    kGzip,
    // Source stream is a zlib-wrapped deflate data.
    kDeflate
  };

  explicit ZLibStreamWrapper(SourceType type);
  ~ZLibStreamWrapper();

  // Returns true if initialization is successful, false otherwise.
  // For instance, this method returns false if there is not enough memory or
  // if there is a version mismatch.
  // Note: Must be called before FilterData.
  bool Init();

  // Filter (i.e. inflate) the data from |input_buffer| into |output_buffer|.
  // Will return the number bytes written to the output buffer or a net::Error.
  // |consumed_bytes| will be set to the number of bytes read from
  // |input_buffer| when expanding.
  // Note: Must call Init() before using this method.
  int FilterData(net::IOBuffer* output_buffer,
                 int output_buffer_size,
                 net::IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_end_reached);

 private:
  enum InputState {
    // Starts processing the input stream. Checks whether the stream is valid
    // and whether a fallback to plain data is needed.
    STATE_START,
    // Gzip header of the input stream is being processed.
    STATE_GZIP_HEADER,
    // Deflate responses may or may not have a zlib header. In this state until
    // enough has been inflated that this stream most likely has a zlib header,
    // or until a zlib header has been added. Data is appended to |replay_data_|
    // in case it needs to be replayed after adding a header.
    STATE_SNIFFING_DEFLATE_HEADER,
    // If a zlib header has to be added to the response, this state will replay
    // data passed to inflate before it was determined that no zlib header was
    // present.
    // See https://crbug.com/677001
    STATE_REPLAY_DATA,
    // The input stream is being decoded.
    STATE_COMPRESSED_BODY,
    // Gzip footer of the input stream is being processed.
    STATE_GZIP_FOOTER,
    // The end of the gzipped body has been reached. If any extra bytes are
    // received, just silently ignore them. Doing this, rather than failing the
    // request or passing the extra bytes alone with the rest of the response
    // body, matches the behavior of other browsers.
    STATE_IGNORING_EXTRA_BYTES,
  };

  bool InsertZlibHeader();

  SourceType type_;
  InputState input_state_;
  InputState replay_state_;
  z_stream_s zlib_stream_;
  // Used to parse the gzip header in gzip stream.
  // It is used when the decoding mode is SourceType::kDeflate.
  net::GZipHeader gzip_header_;

  // While in STATE_SNIFFING_DEFLATE_HEADER, it may be determined that a zlib
  // header needs to be added, and all received data needs to be replayed. In
  // that case, this buffer holds the data to be replayed.
  std::string replay_data_;

  // Tracks how many bytes of gzip footer are yet to be filtered.
  size_t gzip_footer_bytes_left_;

  DISALLOW_COPY_AND_ASSIGN(ZLibStreamWrapper);
};

}  // namespace net

#endif  // NET_FILTER_ZLIB_STREAM_WRAPPER_H_
