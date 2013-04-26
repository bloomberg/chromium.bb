// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "net/base/completion_callback.h"

namespace net {
class FileStream;
class IOBuffer;
}  // namespace net

namespace drive {
namespace internal {

// An interface to dispatch the reading operation. If the file is locally
// cached, LocalReaderProxy defined below will be used. Otherwise (i.e. the
// file is being downloaded from the server), NetworkReaderProxy will be used.
class ReaderProxy {
 public:
  virtual ~ReaderProxy() {}

  // Called from DriveFileStreamReader::Read method.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) = 0;

  // Called when the data from the server is received.
  virtual void OnGetContent(scoped_ptr<std::string> data) = 0;

  // Called when the accessing to the file system is completed.
  virtual void OnCompleted(FileError error) = 0;
};

// The read operation implementation for the locally cached files.
class LocalReaderProxy : public ReaderProxy {
 public:
  // The |file_stream| should be the instance which is already opened.
  // This class takes its ownership.
  explicit LocalReaderProxy(scoped_ptr<net::FileStream> file_stream);
  virtual ~LocalReaderProxy();

  // ReaderProxy overrides.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnGetContent(scoped_ptr<std::string> data) OVERRIDE;
  virtual void OnCompleted(FileError error) OVERRIDE;

 private:
  scoped_ptr<net::FileStream> file_stream_;

  DISALLOW_COPY_AND_ASSIGN(LocalReaderProxy);
};

// The read operation implementation for the file which is being downloaded.
class NetworkReaderProxy : public ReaderProxy {
 public:
  // If the instance is deleted during the download process, it is necessary
  // to cancel the job. |job_canceller| should be the callback to run the
  // cancelling.
  NetworkReaderProxy(
      int64 offset, int64 content_length, const base::Closure& job_canceller);
  virtual ~NetworkReaderProxy();

  // ReaderProxy overrides.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnGetContent(scoped_ptr<std::string> data) OVERRIDE;
  virtual void OnCompleted(FileError error) OVERRIDE;

 private:
  // The data received from the server, but not yet read.
  ScopedVector<std::string> pending_data_;

  // The number of bytes to be skipped.
  int64 remaining_offset_;

  // The number of bytes of remaining data (including the data not yet
  // received from the server).
  int64 remaining_content_length_;

  int error_code_;

  // To support pending Read(), it is necessary to keep its arguments.
  scoped_refptr<net::IOBuffer> buffer_;
  int buffer_length_;
  net::CompletionCallback callback_;

  // Keeps the closure to cancel downloading job if necessary.
  // Will be reset when the job is completed (regardless whether the job is
  // successfully done or not).
  base::Closure job_canceller_;

  DISALLOW_COPY_AND_ASSIGN(NetworkReaderProxy);
};

}  // namespace internal

class DriveFileSystemInterface;
class DriveEntryProto;

// The stream reader for a file in DriveFileSystem. Instances of this class
// should live on IO thread.
class DriveFileStreamReader {
 public:
  // Callback to return the DriveFileSystemInterface instance. This is an
  // injecting point for testing.
  // Note that the callback will be copied between threads (IO and UI), and
  // will be called on UI thread.
  typedef base::Callback<DriveFileSystemInterface*()> DriveFileSystemGetter;

  // Callback to return the result of Initialize().
  typedef base::Callback<void(FileError error,
                              scoped_ptr<DriveEntryProto> entry)>
      InitializeCompletionCallback;

  explicit DriveFileStreamReader(
      const DriveFileSystemGetter& drive_file_system_getter);
  ~DriveFileStreamReader();

  // Returns true if the reader is initialized.
  bool IsInitialized() const;

  // Initializes the stream for the |drive_file_path|.
  // |callback| must not be null.
  // TODO(hidehiko): Support reading range (crbug.com/168258).
  void Initialize(const base::FilePath& drive_file_path,
                  const InitializeCompletionCallback& callback);

  // Reads the data into |buffer| at most |buffer_length|, and returns
  // the number of bytes. If an error happened, returns an error code.
  // If no data is available yet, returns net::ERR_IO_PENDING immediately,
  // and when the data is available the actual Read operation is done
  // and |callback| will be run with the result.
  // The Read() method must not be called before the Initialize() is completed
  // successfully, or if there is pending read operation.
  // Neither |buffer| nor |callback| must be null.
  int Read(net::IOBuffer* buffer, int buffer_length,
           const net::CompletionCallback& callback);

 private:
  // Part of Initialize. Called after GetFileContentByPath's initialization
  // is done.
  void InitializeAfterGetFileContentByPathInitialized(
      const base::FilePath& drive_file_path,
      const InitializeCompletionCallback& callback,
      FileError error,
      scoped_ptr<DriveEntryProto> entry,
      const base::FilePath& local_cache_file_path,
      const base::Closure& cancel_download_closure);

  // Part of Initialize. Called when the local file open process is done.
  void InitializeAfterLocalFileOpen(
      const InitializeCompletionCallback& callback,
      scoped_ptr<DriveEntryProto> entry,
      scoped_ptr<net::FileStream> file_stream,
      int open_result);

  // Called when the data is received from the server.
  void OnGetContent(google_apis::GDataErrorCode error_code,
                    scoped_ptr<std::string> data);

  // Called when GetFileContentByPath is completed.
  void OnGetFileContentByPathCompletion(
      const InitializeCompletionCallback& callback,
      FileError error);

  const DriveFileSystemGetter drive_file_system_getter_;
  scoped_ptr<internal::ReaderProxy> reader_proxy_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveFileStreamReader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveFileStreamReader);
};

// TODO(hidehiko): Add thin wrapper class inheriting
// webkit_blob::FileStreamReader for the DriveFileStreamReader.

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
