// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_

#include <sstream>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/hex_utils.h"
#include "net/spdy/platform/api/spdy_export.h"
#include "net/spdy/platform/api/spdy_string.h"
#include "net/spdy/platform/api/spdy_string_piece.h"

namespace net {

template <typename... Args>
inline SpdyString SpdyStrCatImpl(const Args&... args) {
  std::ostringstream oss;
  int dummy[] = {1, (oss << args, 0)...};
  static_cast<void>(dummy);
  return oss.str();
}

template <typename... Args>
inline void SpdyStrAppendImpl(SpdyString* output, Args... args) {
  output->append(SpdyStrCatImpl(args...));
}

template <typename... Args>
inline SpdyString SpdyStringPrintfImpl(const Args&... args) {
  return base::StringPrintf(std::forward<const Args&>(args)...);
}

template <typename... Args>
inline void SpdyStringAppendFImpl(const Args&... args) {
  base::StringAppendF(std::forward<const Args&>(args)...);
}

inline char SpdyHexDigitToIntImpl(char c) {
  return base::HexDigitToInt(c);
}

inline SpdyString SpdyHexDecodeImpl(SpdyStringPiece data) {
  return HexDecode(data);
}

NET_EXPORT_PRIVATE bool SpdyHexDecodeToUInt32Impl(SpdyStringPiece data,
                                                  uint32_t* out);

inline SpdyString SpdyHexEncodeImpl(const char* bytes, size_t size) {
  return base::ToLowerASCII(base::HexEncode(bytes, size));
}

inline SpdyString SpdyHexEncodeUInt32AndTrimImpl(uint32_t data) {
  return base::StringPrintf("%x", data);
}

inline SpdyString SpdyHexDumpImpl(SpdyStringPiece data) {
  return HexDump(data);
}

}  // namespace net

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_STRING_UTILS_IMPL_H_
