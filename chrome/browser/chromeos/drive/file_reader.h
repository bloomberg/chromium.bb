// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_READER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_READER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "net/base/completion_callback.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace

namespace drive {
namespace util {

// This is simple local file reader implementation focusing on Drive's use
// case. All the operations run on |sequenced_task_runner| asynchronously and
// the result will be notified to the caller via |callback|s on the caller's
// thread.
class FileReader {
 public:
  explicit FileReader(base::SequencedTaskRunner* sequenced_task_runner);
  ~FileReader();

  // Opens the file at |file_path|. The initial position of the read will be
  // at |offset| from the beginning of the file.
  // Upon completion, |callback| will be called.
  // |callback| must not be null.
  void Open(const base::FilePath& file_path,
            int64 offset,
            const net::CompletionCallback& callback);

  // Reads the file and copies the data into |buffer|. |buffer_length|
  // is the length of |buffer|.
  // Upon completion, |callback| will be called with the result.
  // |callback| must not be null.
  void Read(net::IOBuffer* buffer,
            int buffer_length,
            const net::CompletionCallback& callback);

 private:
  // The thin wrapper for the platform file to handle closing correctly.
  class ScopedPlatformFile;

  // Part of Open(). Called after the open() operation task running
  // on blocking pool.
  void OpenAfterBlockingPoolTask(
      const net::CompletionCallback& callback,
      ScopedPlatformFile* result_platform_file,
      int open_result);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  base::PlatformFile platform_file_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileReader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileReader);
};

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_READER_H_
