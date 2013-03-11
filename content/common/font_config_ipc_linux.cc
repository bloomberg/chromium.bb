// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_config_ipc_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"

namespace content {

FontConfigIPC::FontConfigIPC(int fd)
    : fd_(fd) {
}

FontConfigIPC::~FontConfigIPC() {
  close(fd_);
}

bool FontConfigIPC::Match(std::string* result_family,
                          unsigned* result_filefaceid,
                          bool filefaceid_valid, unsigned filefaceid,
                          const std::string& family,
                          const void* characters, size_t characters_bytes,
                          bool* is_bold, bool* is_italic) {
  if (family.length() > kMaxFontFamilyLength)
    return false;

  Pickle request;
  request.WriteInt(METHOD_MATCH);
  request.WriteBool(filefaceid_valid);
  if (filefaceid_valid)
    request.WriteUInt32(filefaceid);

  request.WriteBool(is_bold && *is_bold);
  request.WriteBool(is_bold && *is_italic);

  request.WriteUInt32(characters_bytes);
  if (characters_bytes)
    request.WriteBytes(characters, characters_bytes);

  request.WriteString(family);

  uint8_t reply_buf[512];
  const ssize_t r = UnixDomainSocket::SendRecvMsg(fd_, reply_buf,
                                                  sizeof(reply_buf), NULL,
                                                  request);
  if (r == -1)
    return false;

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  PickleIterator iter(reply);
  bool result;
  if (!reply.ReadBool(&iter, &result))
    return false;
  if (!result)
    return false;

  uint32_t reply_filefaceid;
  std::string reply_family;
  bool resulting_bold, resulting_italic;
  if (!reply.ReadUInt32(&iter, &reply_filefaceid) ||
      !reply.ReadString(&iter, &reply_family) ||
      !reply.ReadBool(&iter, &resulting_bold) ||
      !reply.ReadBool(&iter, &resulting_italic)) {
    return false;
  }

  if (result_filefaceid)
    *result_filefaceid = reply_filefaceid;
  if (result_family)
    *result_family = reply_family;

  if (is_bold)
    *is_bold = resulting_bold;
  if (is_italic)
    *is_italic = resulting_italic;

  return true;
}

int FontConfigIPC::Open(unsigned filefaceid) {
  Pickle request;
  request.WriteInt(METHOD_OPEN);
  request.WriteUInt32(filefaceid);

  int result_fd = -1;
  uint8_t reply_buf[256];
  const ssize_t r = UnixDomainSocket::SendRecvMsg(fd_, reply_buf,
                                                  sizeof(reply_buf),
                                                  &result_fd, request);

  if (r == -1)
    return -1;

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  bool result;
  PickleIterator iter(reply);
  if (!reply.ReadBool(&iter, &result) ||
      !result) {
    if (result_fd)
      close(result_fd);
    return -1;
  }

  return result_fd;
}

}  // namespace content
