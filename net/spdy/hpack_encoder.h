// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_ENCODER_H_
#define NET_SPDY_HPACK_ENCODER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_encoding_context.h"

// An HpackEncoder encodes header sets as outlined in
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

namespace net {

namespace test {
class HpackEncoderPeer;
}  // namespace test

class NET_EXPORT_PRIVATE HpackEncoder {
 public:
  friend class test::HpackEncoderPeer;

  explicit HpackEncoder();
  ~HpackEncoder();

  // Encodes the given header set into the given string. Returns
  // whether or not the encoding was successful.
  bool EncodeHeaderSet(const std::map<std::string, std::string>& header_set,
                       std::string* output);

 private:
  static void CookieToCrumbs(base::StringPiece cookie,
                             std::vector<base::StringPiece>* out);

  uint32 max_string_literal_size_;
  HpackEncodingContext context_;

  DISALLOW_COPY_AND_ASSIGN(HpackEncoder);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_ENCODER_H_
