// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/temporary_file.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace plugin {

TempFile::TempFile(Plugin* plugin, PP_FileHandle handle)
    : plugin_(plugin),
      file_handle_(handle) { }

TempFile::~TempFile() { }

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
