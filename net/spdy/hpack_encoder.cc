// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoder.h"

#include "base/strings/string_util.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;
using std::string;

HpackEncoder::HpackEncoder()
    : max_string_literal_size_(kDefaultMaxStringLiteralSize) {}

HpackEncoder::~HpackEncoder() {}

bool HpackEncoder::EncodeHeaderSet(const std::map<string, string>& header_set,
                                   string* output) {
  // TOOD(jgraettinger): Do more sophisticated encoding.
  HpackOutputStream output_stream(max_string_literal_size_);
  for (std::map<string, string>::const_iterator it = header_set.begin();
       it != header_set.end(); ++it) {
    // TODO(jgraettinger): HTTP/2 requires strict lowercasing of headers,
    // and the permissiveness here isn't wanted. Back this out in upstream.
    if (LowerCaseEqualsASCII(it->first, "cookie")) {
      std::vector<StringPiece> crumbs;
      CookieToCrumbs(it->second, &crumbs);
      for (size_t i = 0; i != crumbs.size(); i++) {
        if (!output_stream.AppendLiteralHeaderNoIndexingWithName(
            it->first, crumbs[i])) {
          return false;
        }
      }
    } else if (!output_stream.AppendLiteralHeaderNoIndexingWithName(
        it->first, it->second)) {
      return false;
    }
  }
  output_stream.TakeString(output);
  return true;
}

void HpackEncoder::CookieToCrumbs(StringPiece cookie,
                                  std::vector<StringPiece>* out) {
  out->clear();
  for (size_t pos = 0;;) {
    size_t end = cookie.find(';', pos);

    if (end == StringPiece::npos) {
      out->push_back(cookie.substr(pos));
      return;
    }
    out->push_back(cookie.substr(pos, end - pos));

    // Consume next space if present.
    pos = end + 1;
    if (pos != cookie.size() && cookie[pos] == ' ') {
      pos++;
    }
  }
}

}  // namespace net
