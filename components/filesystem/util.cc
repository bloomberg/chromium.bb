// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/string.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

// module filesystem has various constants which must line up with enum values
// in base::File::Flags.
static_assert(filesystem::kFlagOpen ==
                  static_cast<uint32>(base::File::FLAG_OPEN),
              "");
static_assert(filesystem::kFlagCreate ==
                  static_cast<uint32>(base::File::FLAG_CREATE),
              "");
static_assert(filesystem::kFlagOpenAlways ==
                  static_cast<uint32>(base::File::FLAG_OPEN_ALWAYS),
              "");
static_assert(filesystem::kCreateAlways ==
                  static_cast<uint32>(base::File::FLAG_CREATE_ALWAYS),
              "");
static_assert(filesystem::kFlagOpenTruncated ==
                  static_cast<uint32>(base::File::FLAG_OPEN_TRUNCATED),
              "");
static_assert(filesystem::kFlagRead ==
                  static_cast<uint32>(base::File::FLAG_READ),
              "");
static_assert(filesystem::kFlagWrite ==
                  static_cast<uint32>(base::File::FLAG_WRITE),
              "");
static_assert(filesystem::kFlagAppend ==
                  static_cast<uint32>(base::File::FLAG_APPEND),
              "");

// filesystem.Error in types.mojom must be the same as base::File::Error.
static_assert(static_cast<int>(filesystem::FILE_ERROR_OK) ==
                  static_cast<int>(base::File::FILE_OK),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_FAILED) ==
                  static_cast<int>(base::File::FILE_ERROR_FAILED),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_IN_USE) ==
                  static_cast<int>(base::File::FILE_ERROR_IN_USE),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_EXISTS) ==
                  static_cast<int>(base::File::FILE_ERROR_EXISTS),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NOT_FOUND) ==
                  static_cast<int>(base::File::FILE_ERROR_NOT_FOUND),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_ACCESS_DENIED) ==
                  static_cast<int>(base::File::FILE_ERROR_ACCESS_DENIED),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_TOO_MANY_OPENED) ==
                  static_cast<int>(base::File::FILE_ERROR_TOO_MANY_OPENED),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NO_MEMORY) ==
                  static_cast<int>(base::File::FILE_ERROR_NO_MEMORY),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NO_SPACE) ==
                  static_cast<int>(base::File::FILE_ERROR_NO_SPACE),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NOT_A_DIRECTORY) ==
                  static_cast<int>(base::File::FILE_ERROR_NOT_A_DIRECTORY),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_INVALID_OPERATION) ==
                  static_cast<int>(base::File::FILE_ERROR_INVALID_OPERATION),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_SECURITY) ==
                  static_cast<int>(base::File::FILE_ERROR_SECURITY),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_ABORT) ==
                  static_cast<int>(base::File::FILE_ERROR_ABORT),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NOT_A_FILE) ==
                  static_cast<int>(base::File::FILE_ERROR_NOT_A_FILE),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_NOT_EMPTY) ==
                  static_cast<int>(base::File::FILE_ERROR_NOT_EMPTY),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_INVALID_URL) ==
                  static_cast<int>(base::File::FILE_ERROR_INVALID_URL),
              "");
static_assert(static_cast<int>(filesystem::FILE_ERROR_IO) ==
                  static_cast<int>(base::File::FILE_ERROR_IO),
              "");

// filesystem.Whence in types.mojom must be the same as base::File::Whence.
static_assert(static_cast<int>(filesystem::WHENCE_FROM_BEGIN) ==
                  static_cast<int>(base::File::FROM_BEGIN),
              "");
static_assert(static_cast<int>(filesystem::WHENCE_FROM_CURRENT) ==
                  static_cast<int>(base::File::FROM_CURRENT),
              "");
static_assert(static_cast<int>(filesystem::WHENCE_FROM_END) ==
                  static_cast<int>(base::File::FROM_END),
              "");

namespace filesystem {

FileError IsWhenceValid(Whence whence) {
  return (whence == WHENCE_FROM_CURRENT || whence == WHENCE_FROM_BEGIN ||
          whence == WHENCE_FROM_END)
             ? FILE_ERROR_OK
             : FILE_ERROR_INVALID_OPERATION;
}

FileError IsOffsetValid(int64_t offset) {
  return (offset >= std::numeric_limits<off_t>::min() &&
          offset <= std::numeric_limits<off_t>::max())
             ? FILE_ERROR_OK
             : FILE_ERROR_INVALID_OPERATION;
}

FileError GetError(const base::File& file) {
  return static_cast<filesystem::FileError>(file.error_details());
}

FileInformationPtr MakeFileInformation(const base::File::Info& info) {
  FileInformationPtr file_info(FileInformation::New());
  file_info->type =
      info.is_directory ? FS_FILE_TYPE_DIRECTORY : FS_FILE_TYPE_REGULAR_FILE;
  file_info->size = info.size;

  file_info->atime = info.last_accessed.ToDoubleT();
  file_info->mtime = info.last_modified.ToDoubleT();
  file_info->ctime = info.creation_time.ToDoubleT();

  return file_info.Pass();
}

FileError ValidatePath(const mojo::String& raw_path,
                       const base::FilePath& filesystem_base,
                       base::FilePath* out) {
  DCHECK(!raw_path.is_null());
  if (!base::IsStringUTF8(raw_path.get()))
    return FILE_ERROR_INVALID_OPERATION;

#if defined(OS_POSIX)
  base::FilePath::StringType path = raw_path;
#elif defined(OS_WIN)
  base::FilePath::StringType path = base::UTF8ToUTF16(raw_path.get());
#endif

  // TODO(erg): This isn't really what we want. FilePath::AppendRelativePath()
  // is closer. We need to deal with entirely hostile apps trying to bust this
  // function to use a possibly maliciously provided |raw_path| to bust out of
  // |filesystem_base|.
  base::FilePath full_path = filesystem_base.Append(path);
  if (full_path.ReferencesParent()) {
    // TODO(erg): For now, if it references a parent, we'll consider this bad.
    return FILE_ERROR_ACCESS_DENIED;
  }

  *out = full_path;
  return FILE_ERROR_OK;
}

}  // namespace filesystem
