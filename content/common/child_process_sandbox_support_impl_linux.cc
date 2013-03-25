// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_sandbox_support_impl_linux.h"

#include <sys/stat.h>

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "content/common/sandbox_linux.h"
#include "third_party/WebKit/Source/Platform/chromium/public/linux/WebFontFamily.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebFontRenderStyle.h"

namespace content {

void GetFontFamilyForCharacters(const uint16_t* utf16,
                                size_t num_utf16,
                                const char* preferred_locale,
                                WebKit::WebFontFamily* family) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_FONT_FAMILY_FOR_CHARS);
  request.WriteInt(num_utf16);
  for (size_t i = 0; i < num_utf16; ++i)
    request.WriteUInt32(utf16[i]);
  request.WriteString(preferred_locale);

  uint8_t buf[512];
  const ssize_t n = UnixDomainSocket::SendRecvMsg(GetSandboxFD(), buf,
                                                  sizeof(buf), NULL, request);

  std::string family_name;
  bool isBold = false;
  bool isItalic = false;
  if (n != -1) {
    Pickle reply(reinterpret_cast<char*>(buf), n);
    PickleIterator pickle_iter(reply);
    if (reply.ReadString(&pickle_iter, &family_name) &&
        reply.ReadBool(&pickle_iter, &isBold) &&
        reply.ReadBool(&pickle_iter, &isItalic)) {
      family->name = family_name;
      family->isBold = isBold;
      family->isItalic = isItalic;
    }
  }
}

void GetRenderStyleForStrike(const char* family, int sizeAndStyle,
                             WebKit::WebFontRenderStyle* out) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_STYLE_FOR_STRIKE);
  request.WriteString(family);
  request.WriteInt(sizeAndStyle);

  uint8_t buf[512];
  const ssize_t n = UnixDomainSocket::SendRecvMsg(GetSandboxFD(), buf,
                                                  sizeof(buf), NULL, request);

  out->setDefaults();
  if (n == -1) {
    return;
  }

  Pickle reply(reinterpret_cast<char*>(buf), n);
  PickleIterator pickle_iter(reply);
  int useBitmaps, useAutoHint, useHinting, hintStyle, useAntiAlias;
  int useSubpixelRendering, useSubpixelPositioning;
  if (reply.ReadInt(&pickle_iter, &useBitmaps) &&
      reply.ReadInt(&pickle_iter, &useAutoHint) &&
      reply.ReadInt(&pickle_iter, &useHinting) &&
      reply.ReadInt(&pickle_iter, &hintStyle) &&
      reply.ReadInt(&pickle_iter, &useAntiAlias) &&
      reply.ReadInt(&pickle_iter, &useSubpixelRendering) &&
      reply.ReadInt(&pickle_iter, &useSubpixelPositioning)) {
    out->useBitmaps = useBitmaps;
    out->useAutoHint = useAutoHint;
    out->useHinting = useHinting;
    out->hintStyle = hintStyle;
    out->useAntiAlias = useAntiAlias;
    out->useSubpixelRendering = useSubpixelRendering;
    out->useSubpixelPositioning = useSubpixelPositioning;
  }
}

int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_MATCH_WITH_FALLBACK);
  request.WriteString(face);
  request.WriteBool(bold);
  request.WriteBool(italic);
  request.WriteUInt32(charset);
  uint8_t reply_buf[64];
  int fd = -1;
  UnixDomainSocket::SendRecvMsg(GetSandboxFD(), reply_buf, sizeof(reply_buf),
                                &fd, request);
  return fd;
}

bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length) {
  if (table == 0) {
    struct stat st;
    if (fstat(fd, &st) < 0)
      return false;
    size_t length = st.st_size;
    if (!output) {
      *output_length = length;
      return true;
    }
    if (*output_length < length)
      return false;
    *output_length = length;
    ssize_t n = HANDLE_EINTR(pread(fd, output, length, 0));
    if (n != static_cast<ssize_t>(length))
      return false;
    return true;
  }

  unsigned num_tables;
  uint8_t num_tables_buf[2];

  ssize_t n = HANDLE_EINTR(pread(fd, &num_tables_buf, sizeof(num_tables_buf),
                           4 /* skip the font type */));
  if (n != sizeof(num_tables_buf))
    return false;

  num_tables = static_cast<unsigned>(num_tables_buf[0]) << 8 |
               num_tables_buf[1];

  // The size in bytes of an entry in the table directory.
  static const unsigned kTableEntrySize = 16;
  scoped_array<uint8_t> table_entries(
      new uint8_t[num_tables * kTableEntrySize]);
  n = HANDLE_EINTR(pread(fd, table_entries.get(), num_tables * kTableEntrySize,
                         12 /* skip the SFNT header */));
  if (n != static_cast<ssize_t>(num_tables * kTableEntrySize))
    return false;

  size_t offset;
  size_t length = 0;
  for (unsigned i = 0; i < num_tables; i++) {
    const uint8_t* entry = table_entries.get() + i * kTableEntrySize;
    if (memcmp(entry, &table, sizeof(table)) == 0) {
      offset = static_cast<size_t>(entry[8]) << 24 |
               static_cast<size_t>(entry[9]) << 16 |
               static_cast<size_t>(entry[10]) << 8  |
               static_cast<size_t>(entry[11]);
      length = static_cast<size_t>(entry[12]) << 24 |
               static_cast<size_t>(entry[13]) << 16 |
               static_cast<size_t>(entry[14]) << 8  |
               static_cast<size_t>(entry[15]);

      break;
    }
  }

  if (!length)
    return false;

  if (!output) {
    *output_length = length;
    return true;
  }

  if (*output_length < length)
    return false;

  *output_length = length;
  n = HANDLE_EINTR(pread(fd, output, length, offset));
  if (n != static_cast<ssize_t>(length))
    return false;

  return true;
}

}  // namespace content
