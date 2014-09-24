// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/tools/package_manager/unpacker.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "third_party/zlib/google/zip.h"

namespace mojo {

Unpacker::Unpacker() {
}

Unpacker::~Unpacker() {
  if (!dir_.empty())
    DeleteFile(dir_, true);
}

bool Unpacker::Unpack(const base::FilePath& zip_file) {
  DCHECK(zip_file_.empty());
  zip_file_ = zip_file;

  DCHECK(dir_.empty());
  if (!CreateNewTempDirectory(base::FilePath::StringType(), &dir_))
    return false;
  if (!zip::Unzip(zip_file, dir_)) {
    dir_ = base::FilePath();
    return false;
  }
  return true;
}

}  // namespace mojo
