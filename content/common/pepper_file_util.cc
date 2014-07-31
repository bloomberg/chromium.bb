// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_file_util.h"

namespace content {

fileapi::FileSystemType PepperFileSystemTypeToFileSystemType(
    PP_FileSystemType type) {
  switch (type) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      return fileapi::kFileSystemTypeTemporary;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      return fileapi::kFileSystemTypePersistent;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      return fileapi::kFileSystemTypeExternal;
    default:
      return fileapi::kFileSystemTypeUnknown;
  }
}

}  // namespace content
