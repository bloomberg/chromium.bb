// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_protocol.h"

#include <stdarg.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"

using base::StringPiece;
using std::string;

namespace net {

CryptoHandshakeMessage::CryptoHandshakeMessage() {}
CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

void CryptoHandshakeMessage::SetTaglist(CryptoTag tag, ...) {
  // Warning, if sizeof(CryptoTag) > sizeof(int) then this function will break
  // because the terminating 0 will only be promoted to int.
  COMPILE_ASSERT(sizeof(CryptoTag) <= sizeof(int),
                 crypto_tag_not_be_larger_than_int_or_varargs_will_break);

  std::vector<CryptoTag> tags;
  va_list ap;

  va_start(ap, tag);
  for (;;) {
    CryptoTag tag = va_arg(ap, CryptoTag);
    if (tag == 0) {
      break;
    }
    tags.push_back(tag);
  }

  // Because of the way that we keep tags in memory, we can copy the contents
  // of the vector and get the correct bytes in wire format. See
  // crypto_protocol.h. This assumes that the system is little-endian.
  SetVector(tag, tags);

  va_end(ap);
}

QuicErrorCode CryptoHandshakeMessage::GetTaglist(CryptoTag tag,
                                                 const CryptoTag** out_tags,
                                                 size_t* out_len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map.end()) {
    ret = QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  } else if (it->second.size() % sizeof(CryptoTag) != 0) {
    ret = QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (ret != QUIC_NO_ERROR) {
    *out_tags = NULL;
    *out_len = 0;
    return ret;
  }

  *out_tags = reinterpret_cast<const CryptoTag*>(it->second.data());
  *out_len = it->second.size() / sizeof(CryptoTag);
  return ret;
}

bool CryptoHandshakeMessage::GetStringPiece(CryptoTag tag,
                                            StringPiece* out) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  if (it == tag_value_map.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetNthValue16(
    CryptoTag tag,
    unsigned index,
    StringPiece* out) const {
  StringPiece value;
  if (!GetStringPiece(tag, &value)) {
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  }

  for (unsigned i = 0;; i++) {
    if (value.empty()) {
      return QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND;
    }
    if (value.size() < 2) {
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(value.data());
    size_t size = static_cast<size_t>(data[0]) |
                  (static_cast<size_t>(data[1]) << 8);
    value.remove_prefix(2);

    if (value.size() < size) {
      return QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
    }

    if (i == index) {
      *out = StringPiece(value.data(), size);
      return QUIC_NO_ERROR;
    }

    value.remove_prefix(size);
  }
}

bool CryptoHandshakeMessage::GetString(CryptoTag tag, string* out) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  if (it == tag_value_map.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

QuicErrorCode CryptoHandshakeMessage::GetUint16(CryptoTag tag,
                                                uint16* out) const {
  return GetPOD(tag, out, sizeof(uint16));
}

QuicErrorCode CryptoHandshakeMessage::GetUint32(CryptoTag tag,
                                                uint32* out) const {
  return GetPOD(tag, out, sizeof(uint32));
}

QuicErrorCode CryptoHandshakeMessage::GetPOD(
    CryptoTag tag, void* out, size_t len) const {
  CryptoTagValueMap::const_iterator it = tag_value_map.find(tag);
  QuicErrorCode ret = QUIC_NO_ERROR;

  if (it == tag_value_map.end()) {
    ret = QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND;
  } else if (it->second.size() != len) {
    ret = QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER;
  }

  if (ret != QUIC_NO_ERROR) {
    memset(out, 0, len);
    return ret;
  }

  memcpy(out, it->second.data(), len);
  return ret;
}

}  // namespace net
