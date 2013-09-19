// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILS_H_

#include "base/files/file_path.h"
#include "base/platform_file.h"

namespace extensions {
namespace image_writer_utils {

// Utility class for writing data to a removable disk.
class ImageWriter {
 public:
  ImageWriter();
  virtual ~ImageWriter();

  // Note: If there is already a device open, it will be closed.
  virtual bool Open(const base::FilePath& path);
  virtual bool Close();

  // Writes from data_block to the device, and returns the amount written or -1
  // on error.
  virtual int Write(const char* data_block, int data_size);

 private:
   base::PlatformFile file_;
   int writes_count_;
};

// Utility class for reading a large file, such as a Chrome OS image.
class ImageReader {
 public:
  ImageReader();
  virtual ~ImageReader();

  // Note: If there is already a file open, it will be closed.
  virtual bool Open(const base::FilePath& path);
  virtual bool Close();
  // Read from the file into data_block.
  virtual int Read(char* data_block, int data_size);
  virtual int64 GetSize();

 private:
  base::PlatformFile file_;
};

} // namespace image_writer_utils
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILS_H_
