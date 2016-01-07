// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/temporary_file.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace plugin {

TempFile::TempFile(Plugin* plugin, PP_FileHandle handle)
    : plugin_(plugin),
      file_handle_(handle) { }

TempFile::~TempFile() { }

nacl::DescWrapper* TempFile::MakeDescWrapper(int nacl_file_flags) {
  base::File file_dup = file_handle_.Duplicate();
  if (!file_dup.IsValid())
    return NULL;
#if defined(OS_WIN)
  int fd = _open_osfhandle(
      reinterpret_cast<intptr_t>(file_dup.TakePlatformFile()),
      _O_RDWR | _O_BINARY | _O_TEMPORARY | _O_SHORT_LIVED);
  if (fd < 0)
    return NULL;
#else
  int fd = file_dup.TakePlatformFile();
#endif
  return plugin_->wrapper_factory()->MakeFileDesc(fd, nacl_file_flags);
}

int32_t TempFile::Open(bool writeable) {
  read_wrapper_.reset(MakeDescWrapper(O_RDONLY));
  if (!read_wrapper_)
    return PP_ERROR_FAILED;

  if (writeable) {
    write_wrapper_.reset(MakeDescWrapper(O_RDWR));
    if (!write_wrapper_)
      return PP_ERROR_FAILED;
  }

  return PP_OK;
}

bool TempFile::Reset() {
  // file_handle_, read_wrapper_ and write_wrapper_ are all backed by the
  // same file handle/descriptor, so resetting the seek position of one
  // will reset them all.
  int64_t newpos = file_handle_.Seek(base::File::FROM_BEGIN, 0);
  return newpos == 0;
}

int64_t TempFile::GetLength() {
  return file_handle_.GetLength();
}

PP_FileHandle TempFile::TakeFileHandle() {
  DCHECK(file_handle_.IsValid());
  return file_handle_.TakePlatformFile();
}

PP_FileHandle TempFile::GetFileHandle() {
  DCHECK(file_handle_.IsValid());
  return file_handle_.GetPlatformFile();
}

}  // namespace plugin
