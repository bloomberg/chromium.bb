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
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkStream.h"

namespace content {

FontConfigIPC::FontConfigIPC(int fd)
    : fd_(fd) {
}

FontConfigIPC::~FontConfigIPC() {
  close(fd_);
}

bool FontConfigIPC::matchFamilyName(const char familyName[],
                                    SkTypeface::Style requestedStyle,
                                    FontIdentity* outFontIdentity,
                                    SkString* outFamilyName,
                                    SkTypeface::Style* outStyle) {
  size_t familyNameLen = familyName ? strlen(familyName) : 0;
  if (familyNameLen > kMaxFontFamilyLength)
    return false;

  Pickle request;
  request.WriteInt(METHOD_MATCH);
  request.WriteData(familyName, familyNameLen);
  request.WriteUInt32(requestedStyle);

  uint8_t reply_buf[2048];
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

  SkString     reply_family;
  FontIdentity reply_identity;
  uint32_t     reply_style;
  if (!skia::ReadSkString(reply, &iter, &reply_family) ||
      !skia::ReadSkFontIdentity(reply, &iter, &reply_identity) ||
      !reply.ReadUInt32(&iter, &reply_style)) {
    return false;
  }

  if (outFontIdentity)
    *outFontIdentity = reply_identity;
  if (outFamilyName)
    *outFamilyName = reply_family;
  if (outStyle)
    *outStyle = static_cast<SkTypeface::Style>(reply_style);

  return true;
}

SkStream* FontConfigIPC::openStream(const FontIdentity& identity) {
  Pickle request;
  request.WriteInt(METHOD_OPEN);
  request.WriteUInt32(identity.fID);

  int result_fd = -1;
  uint8_t reply_buf[256];
  const ssize_t r = UnixDomainSocket::SendRecvMsg(fd_, reply_buf,
                                                  sizeof(reply_buf),
                                                  &result_fd, request);

  if (r == -1)
    return NULL;

  Pickle reply(reinterpret_cast<char*>(reply_buf), r);
  bool result;
  PickleIterator iter(reply);
  if (!reply.ReadBool(&iter, &result) ||
      !result) {
    if (result_fd)
      close(result_fd);
    return NULL;
  }

  return new SkFDStream(result_fd, true);
}

}  // namespace content
