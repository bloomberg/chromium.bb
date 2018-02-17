// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_API_QUIC_URL_UTILS_H_
#define NET_QUIC_PLATFORM_API_QUIC_URL_UTILS_H_

#include "base/macros.h"
#include "net/quic/platform/api/quic_export.h"
#include "net/quic/platform/api/quic_string.h"
#include "net/quic/platform/api/quic_string_piece.h"
#include "net/quic/platform/impl/quic_url_utils_impl.h"

namespace net {

class QUIC_EXPORT_PRIVATE QuicUrlUtils {
 public:
  // Returns hostname, or empty std::string if missing.
  static QuicString HostName(QuicStringPiece url);

  // Returns false if any of these conditions occur: (1) Host name too long; (2)
  // Invalid characters in host name, path or params; (3) Invalid port number
  // (e.g. greater than 65535).
  static bool IsValidUrl(QuicStringPiece url);

  // Returns a canonical, valid URL for a PUSH_PROMISE with the specified
  // ":scheme", ":authority", and ":path" header fields, or an empty
  // string if the resulting URL is not valid or supported.
  static QuicString GetPushPromiseUrl(QuicStringPiece scheme,
                                      QuicStringPiece authority,
                                      QuicStringPiece path);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicUrlUtils);
};

}  // namespace net

#endif  // NET_QUIC_PLATFORM_API_QUIC_URL_UTILS_H_
