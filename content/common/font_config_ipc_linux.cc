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

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/refptr.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace content {

class FontConfigIPC::MappedFontFile
    : public base::RefCountedThreadSafe<MappedFontFile> {
 public:
  explicit MappedFontFile(uint32_t font_id) : font_id_(font_id) {}

  uint32_t font_id() const { return font_id_; }

  bool Initialize(int fd) {
    base::ThreadRestrictions::ScopedAllowIO allow_mmap;
    return mapped_font_file_.Initialize(base::File(fd));
  }

  SkMemoryStream* CreateMemoryStream() {
    DCHECK(mapped_font_file_.IsValid());
    auto data = skia::AdoptRef(SkData::NewWithProc(
        mapped_font_file_.data(), mapped_font_file_.length(),
        &MappedFontFile::ReleaseProc, this));
    if (!data)
      return nullptr;
    AddRef();
    return new SkMemoryStream(data.get());
  }

 private:
  friend class base::RefCountedThreadSafe<MappedFontFile>;

  ~MappedFontFile() {
    auto font_config = static_cast<FontConfigIPC*>(FontConfigIPC::RefGlobal());
    font_config->RemoveMappedFontFile(this);
  }

  static void ReleaseProc(const void* ptr, size_t length, void* context) {
    base::ThreadRestrictions::ScopedAllowIO allow_munmap;
    static_cast<MappedFontFile*>(context)->Release();
  }

  uint32_t font_id_;
  base::MemoryMappedFile mapped_font_file_;
};

void CloseFD(int fd) {
  int err = IGNORE_EINTR(close(fd));
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
  TRACE_EVENT0("sandbox_ipc", "FontConfigIPC::matchFamilyName");
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
  if (!iter.ReadBool(&result))
    return false;
  if (!result)
    return false;

  SkString     reply_family;
  FontIdentity reply_identity;
  uint32_t     reply_style;
  if (!skia::ReadSkString(&iter, &reply_family) ||
      !skia::ReadSkFontIdentity(&iter, &reply_identity) ||
      !iter.ReadUInt32(&reply_style)) {
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

SkStreamAsset* FontConfigIPC::openStream(const FontIdentity& identity) {
  TRACE_EVENT0("sandbox_ipc", "FontConfigIPC::openStream");

  {
    base::AutoLock lock(lock_);
    auto mapped_font_files_it = mapped_font_files_.find(identity.fID);
    if (mapped_font_files_it != mapped_font_files_.end())
      return mapped_font_files_it->second->CreateMemoryStream();
  }

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
  if (!iter.ReadBool(&result) || !result) {
    if (result_fd)
      CloseFD(result_fd);
    return NULL;
  }

  scoped_refptr<MappedFontFile> mapped_font_file =
      new MappedFontFile(identity.fID);
  if (!mapped_font_file->Initialize(result_fd))
    return nullptr;

  {
    base::AutoLock lock(lock_);
    auto mapped_font_files_it =
        mapped_font_files_.insert(std::make_pair(mapped_font_file->font_id(),
                                                 mapped_font_file.get())).first;
    return mapped_font_files_it->second->CreateMemoryStream();
  }
}

void FontConfigIPC::RemoveMappedFontFile(MappedFontFile* mapped_font_file) {
  base::AutoLock lock(lock_);
  mapped_font_files_.erase(mapped_font_file->font_id());
}

}  // namespace content

