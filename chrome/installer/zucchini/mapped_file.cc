// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/mapped_file.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace zucchini {

MappedFileReader::MappedFileReader(const base::FilePath& file_path) {
  bool is_ok = buffer_.Initialize(file_path);
  LOG_IF(ERROR, !is_ok) << "Can't read file: " << file_path.value();
}

MappedFileWriter::MappedFileWriter(const base::FilePath& file_path,
                                   size_t length)
    : file_path_(file_path), delete_behavior_(kManualDeleteOnClose) {
  using base::File;
  File file(file_path_, File::FLAG_CREATE_ALWAYS | File::FLAG_READ |
                            File::FLAG_WRITE | File::FLAG_SHARE_DELETE |
                            File::FLAG_CAN_DELETE_ON_CLOSE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Can't create file: " << file_path_.value();
    return;  // |buffer_| will be uninitialized, and therefore invalid.
  }

#if defined(OS_WIN)
  file_handle_ = file.Duplicate();
  // Tell the OS to delete the file when all handles are closed.
  if (file_handle_.DeleteOnClose(true)) {
    delete_behavior_ = kAutoDeleteOnClose;
  } else {
    LOG(WARNING) << "Failed to mark file for delete-on-close: "
                 << file_path_.value();
  }
#endif  // defined(OS_WIN)

  bool is_ok = buffer_.Initialize(std::move(file), {0, length},
                                  base::MemoryMappedFile::READ_WRITE_EXTEND);
  LOG_IF(ERROR, !is_ok) << "Can't map file: " << file_path_.value();
}

MappedFileWriter::~MappedFileWriter() {
  if (IsValid() && delete_behavior_ == kManualDeleteOnClose &&
      !base::DeleteFile(file_path_, false)) {
    LOG(WARNING) << "Failed to delete file: " << file_path_.value();
  }
}

bool MappedFileWriter::Keep() {
#if defined(OS_WIN)
  if (delete_behavior_ == kAutoDeleteOnClose &&
      !file_handle_.DeleteOnClose(false)) {
    LOG(ERROR) << "Failed to prevent deletion of file: " << file_path_.value();
    return false;
  }
#endif  // defined(OS_WIN)
  delete_behavior_ = kKeep;
  return true;
}

}  // namespace zucchini
