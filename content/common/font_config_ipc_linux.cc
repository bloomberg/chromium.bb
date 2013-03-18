// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_config_ipc_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkStream.h"

namespace content {

// Is given a mmap'd address, and will unmap it in its destructor
class MMapStream : public SkStream {
 public:
  MMapStream(const void* addr, size_t length)
    : memory_(reinterpret_cast<const uint8_t*>(addr))
    , offset_(0)
    , length_(length)
  {}

  virtual ~MMapStream() {
    munmap(const_cast<uint8_t*>(memory_), length_);
  }

  virtual bool rewind() OVERRIDE {
    offset_ = 0;
    return true;
  }

  virtual size_t read(void* buffer, size_t size) OVERRIDE {
    if (!buffer && !size) {
      // This is request for the length of the stream.
      return length_;
    }

    size_t remaining = length_ - offset_;
    if (size > remaining)
      size = remaining;
    if (buffer)
      memcpy(buffer, memory_ + offset_, size);

    offset_ += size;
    return size;
  }

  virtual const void* getMemoryBase() OVERRIDE {
    return memory_;
  }

 private:
  const uint8_t* memory_;
  size_t offset_, length_;
};

// Return a stream from the file descriptor, or NULL on failure.
SkStream* StreamFromFD(int fd) {
  struct stat st;
  if (fstat(fd, &st))
    return NULL;

  void* memory = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (memory == MAP_FAILED)
    return NULL;

  return new MMapStream(memory, st.st_size);
}

void CloseFD(int fd) {
  int err = HANDLE_EINTR(close(fd));
  DCHECK(!err);
}

FontConfigIPC::FontConfigIPC(int fd)
    : fd_(fd) {
}

FontConfigIPC::~FontConfigIPC() {
  CloseFD(fd_);
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
      CloseFD(result_fd);
    return NULL;
  }

  SkStream* stream = StreamFromFD(result_fd);
  CloseFD(result_fd);
  return stream;
}

}  // namespace content

