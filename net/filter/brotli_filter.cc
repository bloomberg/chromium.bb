// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_filter.h"

#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "third_party/brotli/dec/decode.h"

namespace net {

// BrotliFilter applies Brotli content decoding to a data stream.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
//
// BrotliFilter is a subclass of Filter. See the latter's header file filter.h
// for sample usage.
class BrotliFilter : public Filter {
 public:
  BrotliFilter(FilterType type)
      : Filter(type), decoding_status_(DECODING_IN_PROGRESS) {
    BrotliStateInit(&brotli_state_);
  }

  ~BrotliFilter() override { BrotliStateCleanup(&brotli_state_); }

  // Decodes the pre-filter data and writes the output into the |dest_buffer|
  // passed in.
  // The function returns FilterStatus. See filter.h for its description.
  //
  // Upon entry, |*dest_len| is the total size (in number of chars) of the
  // destination buffer. Upon exit, |*dest_len| is the actual number of chars
  // written into the destination buffer.
  //
  // This function will fail if there is no pre-filter data in the
  // |stream_buffer_|. On the other hand, |*dest_len| can be 0 upon successful
  // return. For example, decompressor may process some pre-filter data
  // but not produce output yet.
  FilterStatus ReadFilteredData(char* dest_buffer, int* dest_len) override {
    if (!dest_buffer || !dest_len)
      return Filter::FILTER_ERROR;

    if (decoding_status_ == DECODING_DONE) {
      *dest_len = 0;
      return Filter::FILTER_DONE;
    }

    if (decoding_status_ != DECODING_IN_PROGRESS)
      return Filter::FILTER_ERROR;

    size_t output_buffer_size = base::checked_cast<size_t>(*dest_len);
    size_t input_buffer_size = base::checked_cast<size_t>(stream_data_len_);

    size_t available_in = input_buffer_size;
    const uint8_t* next_in = bit_cast<uint8_t*>(next_stream_data_);
    size_t available_out = output_buffer_size;
    uint8_t* next_out = bit_cast<uint8_t*>(dest_buffer);
    size_t total_out = 0;
    BrotliResult result =
        BrotliDecompressStream(&available_in, &next_in, &available_out,
                               &next_out, &total_out, &brotli_state_);

    CHECK(available_in <= input_buffer_size);
    CHECK(available_out <= output_buffer_size);

    base::CheckedNumeric<size_t> safe_bytes_written(output_buffer_size);
    safe_bytes_written -= available_out;
    int bytes_written =
        base::checked_cast<int>(safe_bytes_written.ValueOrDie());

    switch (result) {
      case BROTLI_RESULT_NEEDS_MORE_OUTPUT:
      // Fall through.
      case BROTLI_RESULT_SUCCESS:
        *dest_len = bytes_written;
        stream_data_len_ = base::checked_cast<int>(available_in);
        next_stream_data_ = bit_cast<char*>(next_in);
        if (result == BROTLI_RESULT_SUCCESS) {
          decoding_status_ = DECODING_DONE;
          return Filter::FILTER_DONE;
        }
        return Filter::FILTER_OK;

      case BROTLI_RESULT_NEEDS_MORE_INPUT:
        *dest_len = bytes_written;
        stream_data_len_ = 0;
        next_stream_data_ = nullptr;
        return Filter::FILTER_NEED_MORE_DATA;

      default:
        decoding_status_ = DECODING_ERROR;
        return Filter::FILTER_ERROR;
    }
  }

 private:
  enum DecodingStatus { DECODING_IN_PROGRESS, DECODING_DONE, DECODING_ERROR };

  // Tracks the status of decoding.
  // This variable is updated only by ReadFilteredData.
  DecodingStatus decoding_status_;

  BrotliState brotli_state_;

  DISALLOW_COPY_AND_ASSIGN(BrotliFilter);
};

Filter* CreateBrotliFilter(Filter::FilterType type_id) {
  return new BrotliFilter(type_id);
}

}  // namespace net
