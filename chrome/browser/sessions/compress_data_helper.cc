// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/compress_data_helper.h"

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include <string>

// static
void CompressDataHelper::CompressAndWriteStringToPickle(const std::string& str,
                                                        int max_bytes,
                                                        Pickle* pickle,
                                                        int* bytes_written) {
  // If there is no data to write, only write the size (0).
  if (str.empty()) {
    pickle->WriteInt(0);
    return;
  }

  bool written = false;
  int max_output_size = max_bytes - *bytes_written;
  if (max_output_size > 0) {
    // We need a non-const char* to pass to the compression algorithm.
    int input_size = str.size();
    scoped_array<char> input(new char[input_size]);
    memcpy(input.get(), str.data(), input_size);

    scoped_array<char> output(new char[max_output_size]);
    memset(output.get(), 0, max_output_size);

    bz_stream stream;
    stream.bzalloc = NULL;
    stream.bzfree = NULL;
    stream.opaque = NULL;
    int result = BZ2_bzCompressInit(&stream,
                                    1,  // 100K block size ( > size of our data)
                                    0,  // quiet
                                    0);  // default work factor
    if (result == BZ_OK) {
      stream.next_in = input.get();
      stream.avail_in = input_size;
      stream.next_out = output.get();
      stream.avail_out = max_output_size;
      do {
        result = BZ2_bzCompress(&stream, BZ_FINISH);
      } while (result == BZ_FINISH_OK);

      // If there wasn't enough space for the output, the result won't be
      // BZ_STREAM_END.
      if (result == BZ_STREAM_END) {
        result = BZ2_bzCompressEnd(&stream);
        DCHECK(stream.total_out_lo32 <=
               static_cast<unsigned>(max_output_size));
        if (result == BZ_OK) {
          // Write the size of the original input, so that the decompression
          // part knows how much memory to allocate.
          pickle->WriteInt(input_size);
          pickle->WriteData(output.get(), stream.total_out_lo32);

          *bytes_written += stream.total_out_lo32;
          written = true;
        }
      } else {
        BZ2_bzCompressEnd(&stream);
      }
    }
  }
  if (!written) {
    // We cannot write any data. Write only the data size (0). The reading part
    // will not try to read any data after seeing the size 0.
    pickle->WriteInt(0);
  }
}

// static
bool CompressDataHelper::ReadAndDecompressStringFromPickle(const Pickle& pickle,
                                                           PickleIterator* iter,
                                                           std::string* str) {
  // Read the size of the original data.
  int original_size = 0;
  if (!pickle.ReadLength(iter, &original_size))
    return false;

  if (original_size == 0) {
    // No data to decompress.
    return true;
  }

  // Read the compressed data.
  const char* compressed_data = NULL;
  int compressed_size = 0;
  if (!pickle.ReadData(iter, &compressed_data, &compressed_size))
    return false;

  scoped_array<char> original_data(new char[original_size]);
  memset(original_data.get(), 0, original_size);

  // We need a non-const char* to pass to the decompression algorithm.
  scoped_array<char> compressed_data_copy(new char[compressed_size]);
  memcpy(compressed_data_copy.get(), compressed_data, compressed_size);

  bz_stream stream;
  stream.bzalloc = NULL;
  stream.bzfree = NULL;
  stream.opaque = NULL;

  int result = BZ2_bzDecompressInit(&stream, 0, 0);
  if (result == BZ_OK) {
    stream.next_in = compressed_data_copy.get();
    stream.avail_in = compressed_size;
    stream.next_out = original_data.get();
    stream.avail_out = original_size;

    do {
      // We should know exactly how much space to allocate for the result, thus
      // no out-of-space errors should happen.
      DCHECK(stream.avail_out > 0);
      result = BZ2_bzDecompress(&stream);
    } while (result == BZ_OK);

    DCHECK(result == BZ_STREAM_END);
    if (result == BZ_STREAM_END) {
      result = BZ2_bzDecompressEnd(&stream);
      if (result == BZ_OK) {
        str->assign(original_data.get(), original_size);
      }
    } else {
      BZ2_bzDecompressEnd(&stream);
    }
  }
  return !str->empty();
}
