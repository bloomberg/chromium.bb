// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_
#define CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace image_writer {

class ImageWriterHandler;

// Manages a write within the utility thread.  This class holds all the state
// around the writing and communicates with the ImageWriterHandler to dispatch
// messages.
class ImageWriter : public base::SupportsWeakPtr<ImageWriter> {
 public:
  explicit ImageWriter(ImageWriterHandler* handler);
  virtual ~ImageWriter();

  // Starts a write from |image_path| to |device_path|.
  void Write(const base::FilePath& image_path,
             const base::FilePath& device_path);
  // Starts verifying that |image_path| and |device_path| have the same size and
  // contents.
  void Verify(const base::FilePath& image_path,
              const base::FilePath& device_path);
  // Cancels any pending writes or verifications.
  void Cancel();

  // Returns whether an operation is in progress.
  bool IsRunning() const;

 private:
  // Convenience wrappers.
  void PostTask(const base::Closure& task);
  void PostProgress(int64 progress);
  void Error(const std::string& message);

  // Work loops.
  void WriteChunk();
  void VerifyChunk();

  // Cleans up file handles.
  void CleanUp();

  base::FilePath image_path_;
  base::FilePath device_path_;

  base::PlatformFile image_file_;
  base::PlatformFile device_file_;
  int64 bytes_processed_;

  ImageWriterHandler* handler_;
};

}  // namespace image_writer

#endif  // CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_
