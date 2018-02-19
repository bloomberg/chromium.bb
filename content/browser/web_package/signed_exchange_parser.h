// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PARSER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PARSER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "content/browser/web_package/signed_exchange_header.h"
#include "content/common/content_export.h"

namespace content {

// SignedExchangeParser is a streaming parser for signed-exchange envelope
// format.
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.5
class CONTENT_EXPORT SignedExchangeParser {
 public:
  using HeaderCallback = base::OnceCallback<void(const SignedExchangeHeader&)>;

  explicit SignedExchangeParser(HeaderCallback header_callback);
  ~SignedExchangeParser();

  // ConsumeResult is used to communicate the parser state based on what action
  // the callee should take.  See also the "Usage example" code in TryConsume()
  // on how to interpret the values.
  enum class ConsumeResult {
    // The parser successfully finished parsing the entire item/HTXG, and
    // ensured that there are no extraneous data.
    SUCCESS,
    // The parser aborted processing input due to invalid data.
    PARSE_ERROR,
    // The parser can yield more output, but needs more data in order to make
    // progress. The caller needs to call again with new |input| containing
    // unconsumed bytes from the previous call and some new data.
    // Note that |output| buffer can also be populated with new data
    // in this case, so caller needs to check |written| and drain it.
    MORE_INPUT_NEEDED,
    // The parser sees more payload in the byte stream, but the |output| buffer
    // was too short to write entire payload. The caller is expected to drain
    // |output| buffer and call again, along with the consumed bytes |input|
    // buffer shift just like done for MORE_INPUT_NEEDED.
    MORE_OUTPUT_AVAILABLE,
  };

  // Try to consume an encoded signed-exchange byte stream and make progress.
  //
  // SignedExchangeParser is a streaming parser.
  // e.g. It doesn't require the entire signed-exchange at once, and you are
  // allowed to pass in encoded signed-exchange byte stream in chunks.
  //
  // A signed-exchange byte stream is provided via |input|, and number of
  // |input| bytes consumed in the call is written to |consumed_bytes|.
  // The parser may not consume all the bytes in |input|, and expects that
  // more bytes are appended to the unconsumed bytes when called next time.
  //
  // TryConsume() will synchronously call the HeaderCallback once it is done
  // processing the header part.
  // Once the parser reaches payload part, the payload is extracted to
  // |output|. Number of payload bytes written to |output| buffer is provided
  // to |written|.
  //
  // To complete the parse of a signed-exchange, multiple calls to TryConsume
  // is expected. Please refer to example code below and ConsumeResult enum
  // comments for details.
  //
  // Usage example:
  //
  // std::vector<uint8_t> input;
  // size_t input_valid = read(fd, input.data(), input.size();
  // std::vector<uint8_t> output;
  //
  // for (;;) {
  //   size_t consumed_bytes, written;
  //   auto cr = parser.TryConsume(input.first(input_valid), &consumed_bytes,
  //                               output, &written);
  //   write(outfd, output.data(), output.data() + written);
  //
  //   switch (cr) {
  //     case ConsumeResult::SUCCESS:
  //       return true;
  //     case ConsumeResult::PARSE_ERROR:
  //       return false;
  //     default:
  //   }
  //
  //   // Prepare input that didn't get consumed in the previous TryConsume()
  //   // call for next TryConsume() call.
  //   size_t unconsumed_input = input.size() - consumed_bytes;
  //   if (unconsumed_input)
  //     memmove(input.data(), input.data() + consumed_bytes, unconsumed_input);
  //
  //   switch (cr) {
  //     case ConsumeResult::MORE_INPUT_NEEDED:
  //       input_valid = read(infd, input.data() + unconsumed_input,
  //       consumed_bytes);
  //       // read error handling omitted for brevity.
  //       break;
  //     case ConsumeResult::MORE_OUTPUT_AVAILABLE:
  //       break;
  //     default:
  //       NOTREACHED();
  //   }
  // }
  ConsumeResult TryConsume(base::span<const uint8_t> input,
                           size_t* consumed_bytes,
                           base::span<uint8_t> output,
                           size_t* written);

 private:
  HeaderCallback header_callback_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeParser);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PARSER_H_
