// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoder.h"

#include "net/spdy/hpack_output_stream.h"

namespace net {

using std::string;

HpackEncoder::HpackEncoder(uint32 max_string_literal_size)
    : max_string_literal_size_(max_string_literal_size) {}

HpackEncoder::~HpackEncoder() {}

bool HpackEncoder::EncodeHeaderSet(const std::map<string, string>& header_set,
                                   string* output) {
  // TOOD(akalin): Do more sophisticated encoding.
  HpackOutputStream output_stream(max_string_literal_size_);
  for (std::map<string, string>::const_iterator it = header_set.begin();
       it != header_set.end(); ++it) {
    // TODO(akalin): Clarify in the spec that encoding with the name
    // as a literal is OK even if the name already exists in the
    // header table.
    if (!output_stream.AppendLiteralHeaderNoIndexingWithName(
            it->first, it->second)) {
      return false;
    }
  }
  output_stream.TakeString(output);
  return true;
}

}  // namespace net
