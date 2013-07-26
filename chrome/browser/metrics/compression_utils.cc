// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/compression_utils.h"

#include <vector>

#include "base/basictypes.h"
#include "third_party/zlib/zlib.h"

namespace {

// The difference in bytes between a zlib header and a gzip header.
const size_t kGzipZlibHeaderDifferenceBytes = 16;

// Pass an integer greater than the following get a gzip header instead of a
// zlib header when calling deflateInit2_.
const int kWindowBitsToGetGzipHeader = 16;

// This describes the amount of memory zlib uses to compress data. It can go
// from 1 to 9, with 8 being the default. For details, see:
// http://www.zlib.net/manual.html (search for memLevel).
const int kZlibMemoryLevel = 8;

// This code is taken almost verbatim from third_party/zlib/compress.c. The only
// difference is deflateInit2_ is called which sets the window bits to be > 16.
// That causes a gzip header to be emitted rather than a zlib header.
int GzipCompressHelper(Bytef* dest,
                       uLongf* dest_length,
                       const Bytef* source,
                       uLong source_length) {
    z_stream stream;

    stream.next_in = bit_cast<Bytef*>(source);
    stream.avail_in = static_cast<uInt>(source_length);
    stream.next_out = dest;
    stream.avail_out = static_cast<uInt>(*dest_length);
    if (static_cast<uLong>(stream.avail_out) != *dest_length)
      return Z_BUF_ERROR;

    stream.zalloc = static_cast<alloc_func>(0);
    stream.zfree = static_cast<free_func>(0);
    stream.opaque = static_cast<voidpf>(0);

    gz_header gzip_header;
    memset(&gzip_header, 0, sizeof(gzip_header));
    int err = deflateInit2_(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            MAX_WBITS + kWindowBitsToGetGzipHeader,
                            kZlibMemoryLevel,
                            Z_DEFAULT_STRATEGY,
                            ZLIB_VERSION,
                            sizeof(z_stream));
    if (err != Z_OK)
      return err;

    err = deflateSetHeader(&stream, &gzip_header);
    if (err != Z_OK)
      return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *dest_length = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}
}  // namespace

namespace chrome {

bool GzipCompress(const std::string& input, std::string* output) {
  std::vector<Bytef> compressed_data(kGzipZlibHeaderDifferenceBytes +
                                     compressBound(input.size()));

  uLongf compressed_size = compressed_data.size();
  if (GzipCompressHelper(&compressed_data.front(),
                         &compressed_size,
                         bit_cast<const Bytef*>(input.data()),
                         input.size()) != Z_OK)
    return false;

  compressed_data.resize(compressed_size);
  output->assign(compressed_data.begin(), compressed_data.end());
  return true;
}
}  // namespace chrome
