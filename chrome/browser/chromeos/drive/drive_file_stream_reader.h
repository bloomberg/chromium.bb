// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "net/base/completion_callback.h"
#include "webkit/blob/file_stream_reader.h"

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

  // Called when an error is found, during the network downloading.
  virtual void OnError(DriveFileError error) = 0;
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
  virtual void OnError(DriveFileError error) OVERRIDE;

 private:
  scoped_ptr<net::FileStream> file_stream_;

  DISALLOW_COPY_AND_ASSIGN(LocalReaderProxy);
};

// The read operation implementation for the file which is being downloaded.
class NetworkReaderProxy : public ReaderProxy {
 public:
  explicit NetworkReaderProxy(int64 content_length);
  virtual ~NetworkReaderProxy();

  // ReaderProxy overrides.
  virtual int Read(net::IOBuffer* buffer, int buffer_length,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnGetContent(scoped_ptr<std::string> data) OVERRIDE;
  virtual void OnError(DriveFileError error) OVERRIDE;

 private:
  // The data received from the server, but not yet read.
  ScopedVector<std::string> pending_data_;

  // The number of bytes of remaining data (including the data not yet
  // received from the server).
  int64 remaining_content_length_;

  int error_code_;

  // To support pending Read(), it is necessary to keep its arguments.
  scoped_refptr<net::IOBuffer> buffer_;
  int buffer_length_;
  net::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(NetworkReaderProxy);
};

}  // namespace internal

// TODO(hidehiko): Simplify the interface by getting rid of
// webkit_blob::FileStreamReader inheritance.
class DriveFileStreamReader : public webkit_blob::FileStreamReader {
 public:
  DriveFileStreamReader();
  virtual ~DriveFileStreamReader();

  // webkit_blob::FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFileStreamReader);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_STREAM_READER_H_
