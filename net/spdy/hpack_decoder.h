// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_DECODER_H_
#define NET_SPDY_HPACK_DECODER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_encoding_context.h"
#include "net/spdy/hpack_input_stream.h"  // For HpackHeaderPairVector.

namespace net {

// An HpackDecoder decodes header sets as outlined in
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05
// .
class NET_EXPORT_PRIVATE HpackDecoder {
 public:
  explicit HpackDecoder(uint32 max_string_literal_size);
  ~HpackDecoder();

  // Decodes the given string into the given header set. Returns
  // whether or not the decoding was successful.
  //
  // TODO(akalin): Emit the headers via a callback/delegate instead of
  // putting them into a vector.
  bool DecodeHeaderSet(base::StringPiece input,
                       HpackHeaderPairVector* header_list);

  // Accessors for testing.

  bool DecodeNextNameForTest(HpackInputStream* input_stream,
                             base::StringPiece* next_name) {
    return DecodeNextName(input_stream, next_name);
  }

 private:
  const uint32 max_string_literal_size_;
  HpackEncodingContext context_;

  // Tries to process the next header representation and maybe emit
  // headers into |header_list| according to it. Returns true if
  // successful, or false if an error was encountered.
  bool ProcessNextHeaderRepresentation(
      HpackInputStream* input_stream,
      HpackHeaderPairVector* header_list);

  bool DecodeNextName(HpackInputStream* input_stream,
                      base::StringPiece* next_name);
  bool DecodeNextValue(HpackInputStream* input_stream,
                       base::StringPiece* next_name);

  DISALLOW_COPY_AND_ASSIGN(HpackDecoder);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_DECODER_H_
