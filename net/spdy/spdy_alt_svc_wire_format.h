// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains data structures and utility functions used for serializing
// and parsing alternative service header values, common to HTTP/1.1 header
// fields and HTTP/2 and QUIC ALTSVC frames.  See specification at
// https://tools.ietf.org/id/draft-ietf-httpbis-alt-svc-06.html

#ifndef NET_SPDY_SPDY_ALT_SVC_WIRE_FORMAT_H_
#define NET_SPDY_SPDY_ALT_SVC_WIRE_FORMAT_H_

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

using base::StringPiece;

namespace net {

namespace test {
class SpdyAltSvcWireFormatPeer;
}  // namespace test

class NET_EXPORT_PRIVATE SpdyAltSvcWireFormat {
 public:
  friend class test::SpdyAltSvcWireFormatPeer;
  static bool ParseHeaderFieldValue(StringPiece value,
                                    std::string* protocol_id,
                                    std::string* host,
                                    uint16* port,
                                    uint32* max_age,
                                    double* p);
  static std::string SerializeHeaderFieldValue(const std::string& protocol_id,
                                               const std::string& host,
                                               uint16 port,
                                               uint32 max_age,
                                               double p);

 private:
  static void SkipWhiteSpace(StringPiece::const_iterator* c,
                             StringPiece::const_iterator end);
  static bool PercentDecode(StringPiece::const_iterator c,
                            StringPiece::const_iterator end,
                            std::string* output);
  static bool ParseAltAuthority(StringPiece::const_iterator c,
                                StringPiece::const_iterator end,
                                std::string* host,
                                uint16* port);
  static bool ParsePositiveInteger16(StringPiece::const_iterator c,
                                     StringPiece::const_iterator end,
                                     uint16* value);
  static bool ParsePositiveInteger32(StringPiece::const_iterator c,
                                     StringPiece::const_iterator end,
                                     uint32* value);
  static bool ParseProbability(StringPiece::const_iterator c,
                               StringPiece::const_iterator end,
                               double* p);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_ALT_SVC_WIRE_FORMAT_H_
