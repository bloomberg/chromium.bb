// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInterface;

// Implements a streamed file writer. It is lazily initialized by the first call
// to Write().
class FileStreamWriter : public fileapi::FileStreamWriter {
 public:
  typedef base::Callback<
      void(base::WeakPtr<ProvidedFileSystemInterface> file_system,
           const base::FilePath& file_path,
           int file_handle,
           base::File::Error result)> OpenFileCompletedCallback;

  FileStreamWriter(const fileapi::FileSystemURL& url, int64 initial_offset);

  virtual ~FileStreamWriter();

  // fileapi::FileStreamWriter overrides.
  virtual int Write(net::IOBuffer* buf,
                    int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;
  virtual int Flush(const net::CompletionCallback& callback) OVERRIDE;

 private:
  // State of the file stream writer.
  enum State { NOT_INITIALIZED, INITIALIZING, INITIALIZED, FAILED };

  void OnWriteFileCompleted(int buffer_length,
                            const net::CompletionCallback& callback,
                            base::File::Error result);

  // Called when Write() operation is completed with either a success of an
  // error.
  void OnWriteCompleted(net::CompletionCallback callback, int result);

  // Initializes the writer by opening the file. When completed with success,
  // runs the |pending_closure|. Otherwise, calls the |error_callback|.
  void Initialize(const base::Closure& pending_closure,
                  const net::CompletionCallback& error_callback);

  // Called when opening a file is completed with either a success or an error.
  void OnOpenFileCompleted(
      const base::Closure& pending_closure,
      const net::CompletionCallback& error_callback,
      base::WeakPtr<ProvidedFileSystemInterface> file_system,
      const base::FilePath& file_path,
      int file_handle,
      base::File::Error result);

  // Same as Write(), but called after initializing is completed.
  void WriteAfterInitialized(scoped_refptr<net::IOBuffer> buffer,
                             int buffer_length,
                             const net::CompletionCallback& callback);

  fileapi::FileSystemURL url_;
  int64 current_offset_;
  State state_;

  // Set during initialization (in case of a success).
  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
  base::FilePath file_path_;
  int file_handle_;

  base::WeakPtrFactory<FileStreamWriter> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileStreamWriter);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_
