// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ntlm/ntlm_buffer_writer.h"

#include <string.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

template class std::basic_string<uint8_t>;

namespace net {
namespace ntlm {

NtlmBufferWriter::NtlmBufferWriter(size_t buffer_len)
    : buffer_(buffer_len, 0), cursor_(0) {}

NtlmBufferWriter::~NtlmBufferWriter() {}

bool NtlmBufferWriter::CanWrite(size_t len) const {
  if (!GetBufferPtr())
    return false;

  DCHECK_LE(GetCursor(), GetLength());

  if (len == 0)
    return true;

  return (len <= GetLength()) && (GetCursor() <= GetLength() - len);
}

bool NtlmBufferWriter::WriteUInt16(uint16_t value) {
  return WriteUInt<uint16_t>(value);
}

bool NtlmBufferWriter::WriteUInt32(uint32_t value) {
  return WriteUInt<uint32_t>(value);
}

bool NtlmBufferWriter::WriteUInt64(uint64_t value) {
  return WriteUInt<uint64_t>(value);
}

bool NtlmBufferWriter::WriteFlags(NegotiateFlags flags) {
  return WriteUInt32(static_cast<uint32_t>(flags));
}

bool NtlmBufferWriter::WriteBytes(const uint8_t* buffer, size_t len) {
  if (!CanWrite(len))
    return false;

  memcpy(reinterpret_cast<void*>(GetBufferPtrAtCursor()),
         reinterpret_cast<const void*>(buffer), len);

  AdvanceCursor(len);
  return true;
}

bool NtlmBufferWriter::WriteBytes(base::StringPiece bytes) {
  return WriteBytes(reinterpret_cast<const uint8_t*>(bytes.data()),
                    bytes.length());
}

bool NtlmBufferWriter::WriteZeros(size_t count) {
  if (!CanWrite(count))
    return false;

  memset(GetBufferPtrAtCursor(), 0, count);
  AdvanceCursor(count);
  return true;
}

bool NtlmBufferWriter::WriteSecurityBuffer(SecurityBuffer sec_buf) {
  return WriteUInt16(sec_buf.length) && WriteUInt16(sec_buf.length) &&
         WriteUInt32(sec_buf.offset);
}

bool NtlmBufferWriter::WriteUtf8String(const std::string& str) {
  return WriteBytes(reinterpret_cast<const uint8_t*>(str.c_str()),
                    str.length());
}

bool NtlmBufferWriter::WriteUtf16AsUtf8String(const base::string16& str) {
  std::string utf8 = base::UTF16ToUTF8(str);
  return WriteUtf8String(utf8);
}

bool NtlmBufferWriter::WriteUtf8AsUtf16String(const std::string& str) {
  base::string16 unicode = base::UTF8ToUTF16(str);
  return WriteUtf16String(unicode);
}

bool NtlmBufferWriter::WriteUtf16String(const base::string16& str) {
  size_t num_bytes = str.length() * 2;
  if (!CanWrite(num_bytes))
    return false;

#if defined(ARCH_CPU_BIG_ENDIAN)
  uint8_t* ptr = reinterpret_cast<uint8_t*>(GetBufferPtrAtCursor());

  for (int i = 0; i < num_bytes; i += 2) {
    ptr[i] = str[i / 2] & 0xff;
    ptr[i + 1] = str[i / 2] >> 8;
  }
#else
  memcpy(reinterpret_cast<void*>(GetBufferPtrAtCursor()), str.c_str(),
         num_bytes);

#endif

  AdvanceCursor(num_bytes);
  return true;
}

bool NtlmBufferWriter::WriteSignature() {
  return WriteBytes(kSignature, kSignatureLen);
}

bool NtlmBufferWriter::WriteMessageType(MessageType message_type) {
  return WriteUInt32(static_cast<uint32_t>(message_type));
}

bool NtlmBufferWriter::WriteMessageHeader(MessageType message_type) {
  return WriteSignature() && WriteMessageType(message_type);
}

template <typename T>
bool NtlmBufferWriter::WriteUInt(T value) {
  size_t int_size = sizeof(T);
  if (!CanWrite(int_size))
    return false;

  for (size_t i = 0; i < int_size; i++) {
    GetBufferPtrAtCursor()[i] = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  }

  AdvanceCursor(int_size);
  return true;
}

void NtlmBufferWriter::SetCursor(size_t cursor) {
  DCHECK(GetBufferPtr() && cursor <= GetLength());

  cursor_ = cursor;
}

}  // namespace ntlm
}  // namespace net
