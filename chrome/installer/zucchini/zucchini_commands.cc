// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_commands.h"

#include <iostream>
#include <ostream>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/crc32.h"
#include "chrome/installer/zucchini/io_utils.h"

namespace {

/******** MappedFileReader ********/

// A file reader wrapper.
class MappedFileReader {
 public:
  explicit MappedFileReader(const base::FilePath& file_name) {
    is_ok_ = buffer_.Initialize(file_name);
    if (!is_ok_)  // This is also triggered if |file_name| is an empty file.
      LOG(ERROR) << "Can't read file: " << file_name.value();
  }
  bool is_ok() const { return is_ok_; }
  const uint8_t* data() const { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::ConstBufferView region() const { return {data(), length()}; }

 private:
  bool is_ok_;
  base::MemoryMappedFile buffer_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileReader);
};

/******** MappedFileWriter ********/

// A file writer wrapper.
class MappedFileWriter {
 public:
  MappedFileWriter(const base::FilePath& file_name, size_t length) {
    using base::File;
    is_ok_ = buffer_.Initialize(
        File(file_name,
             File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE),
        {0, static_cast<int64_t>(length)},
        base::MemoryMappedFile::READ_WRITE_EXTEND);
    if (!is_ok_)
      LOG(ERROR) << "Can't create file: " << file_name.value();
  }
  bool is_ok() const { return is_ok_; }
  uint8_t* data() { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::MutableBufferView region() { return {data(), length()}; }

 private:
  bool is_ok_;
  base::MemoryMappedFile buffer_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileWriter);
};

}  // namespace

zucchini::status::Code MainGen(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());
  MappedFileReader old_image(params.file_paths[0]);
  if (!old_image.is_ok())
    return zucchini::status::kStatusFileReadError;
  MappedFileReader new_image(params.file_paths[1]);
  if (!new_image.is_ok())
    return zucchini::status::kStatusFileReadError;

  // TODO(etiennep): Implement.

  // Dummy output as placeholder.
  MappedFileWriter patch(params.file_paths[2], 256);
  if (!patch.is_ok())
    return zucchini::status::kStatusFileWriteError;

  return zucchini::status::kStatusSuccess;
}

zucchini::status::Code MainApply(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());
  MappedFileReader old_image(params.file_paths[0]);
  if (!old_image.is_ok())
    return zucchini::status::kStatusFileReadError;
  MappedFileReader patch(params.file_paths[1]);
  if (!patch.is_ok())
    return zucchini::status::kStatusFileReadError;

  // TODO(etiennep): Implement.

  // Dummy output as placeholder.
  MappedFileWriter new_image(params.file_paths[2], 256);
  if (!new_image.is_ok())
    return zucchini::status::kStatusFileWriteError;

  return zucchini::status::kStatusSuccess;
}

zucchini::status::Code MainCrc32(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  MappedFileReader image(params.file_paths[0]);
  if (!image.is_ok())
    return zucchini::status::kStatusFileReadError;

  uint32_t crc =
      zucchini::CalculateCrc32(image.data(), image.data() + image.length());
  params.out << "CRC32: " << zucchini::AsHex<8>(crc) << std::endl;
  return zucchini::status::kStatusSuccess;
}
