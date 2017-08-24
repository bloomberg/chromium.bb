// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_
#define CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "chrome/installer/zucchini/buffer_view.h"

namespace zucchini {

// A file reader wrapper.
class MappedFileReader {
 public:
  // Opens |file_path| and maps it to memory for reading.
  explicit MappedFileReader(const base::FilePath& file_path);

  bool IsValid() const { return buffer_.IsValid(); }
  const uint8_t* data() const { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::ConstBufferView region() const { return {data(), length()}; }

 private:
  base::MemoryMappedFile buffer_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileReader);
};

// A file writer wrapper. The target file is deleted on destruction unless
// Keep() is called.
class MappedFileWriter {
 public:
  // Creates |file_path| and maps it to memory for writing.
  MappedFileWriter(const base::FilePath& file_path, size_t length);
  ~MappedFileWriter();

  bool IsValid() const { return buffer_.IsValid(); }
  uint8_t* data() { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::MutableBufferView region() { return {data(), length()}; }

  // Indicates that the file should not be deleted on destruction. Returns true
  // iff the operation succeeds.
  bool Keep();

 private:
  enum OnCloseDeleteBehavior {
    kKeep,
    kAutoDeleteOnClose,
    kManualDeleteOnClose
  };

  base::FilePath file_path_;
  base::File file_handle_;
  base::MemoryMappedFile buffer_;
  OnCloseDeleteBehavior delete_behavior_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileWriter);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_
