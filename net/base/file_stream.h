// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream, a basic interface for reading and writing files
// synchronously or asynchronously with support for seeking to an offset.
// Note that even when used asynchronously, only one operation is supported at
// a time.

#ifndef NET_BASE_FILE_STREAM_H_
#define NET_BASE_FILE_STREAM_H_

#include "base/files/file.h"
#include "base/platform_file.h"
#include "base/task_runner.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream_whence.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"

namespace base {
class FilePath;
}

namespace net {

class IOBuffer;

class NET_EXPORT FileStream {
 public:
  // Creates a |FileStream| with a new |BoundNetLog| (based on |net_log|)
  // attached.  |net_log| may be NULL if no logging is needed.
  // Uses |task_runner| for asynchronous operations.
  FileStream(net::NetLog* net_log,
             const scoped_refptr<base::TaskRunner>& task_runner);

  // Same as above, but runs async tasks in base::WorkerPool.
  explicit FileStream(net::NetLog* net_log);

  // Construct a FileStream with an existing file handle and opening flags.
  // |file| is valid file handle.
  // |flags| is a bitfield of base::PlatformFileFlags when the file handle was
  // opened.
  // |net_log| is the net log pointer to use to create a |BoundNetLog|.  May be
  // NULL if logging is not needed.
  // Uses |task_runner| for asynchronous operations.
  // Note: the new FileStream object takes ownership of the PlatformFile and
  // will close it on destruction.
  // This constructor is deprecated.
  // TODO(rvargas): remove all references to PlatformFile.
  FileStream(base::PlatformFile file,
             int flags,
             net::NetLog* net_log,
             const scoped_refptr<base::TaskRunner>& task_runner);

  // Same as above, but runs async tasks in base::WorkerPool.
  // This constructor is deprecated.
  FileStream(base::PlatformFile file, int flags, net::NetLog* net_log);

  // Non-deprecated versions of the previous two constructors.
  FileStream(base::File file,
             net::NetLog* net_log,
             const scoped_refptr<base::TaskRunner>& task_runner);
  FileStream(base::File file, net::NetLog* net_log);

  // The underlying file is closed automatically.
  virtual ~FileStream();

  // Call this method to open the FileStream asynchronously.  The remaining
  // methods cannot be used unless the file is opened successfully. Returns
  // ERR_IO_PENDING if the operation is started. If the operation cannot be
  // started then an error code is returned.
  //
  // Once the operation is done, |callback| will be run on the thread where
  // Open() was called, with the result code. open_flags is a bitfield of
  // base::PlatformFileFlags.
  //
  // If the file stream is not closed manually, the underlying file will be
  // automatically closed when FileStream is destructed in an asynchronous
  // manner (i.e. the file stream is closed in the background but you don't
  // know when).
  virtual int Open(const base::FilePath& path, int open_flags,
                   const CompletionCallback& callback);

  // Returns ERR_IO_PENDING and closes the file asynchronously, calling
  // |callback| when done.
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  virtual int Close(const CompletionCallback& callback);

  // Returns true if Open succeeded and Close has not been called.
  virtual bool IsOpen() const;

  // Adjust the position from where data is read asynchronously.
  // Upon success, ERR_IO_PENDING is returned and |callback| will be run
  // on the thread where Seek() was called with the the stream position
  // relative to the start of the file.  Otherwise, an error code is returned.
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  virtual int Seek(Whence whence, int64 offset,
                   const Int64CompletionCallback& callback);

  // Call this method to read data from the current stream position
  // asynchronously. Up to buf_len bytes will be copied into buf.  (In
  // other words, partial reads are allowed.)  Returns the number of bytes
  // copied, 0 if at end-of-file, or an error code if the operation could
  // not be performed.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the read could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Read() was called, when the
  // read has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous read in progress.  That will cancel the read and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened WRITE_ONLY.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback);

  // Call this method to write data at the current stream position
  // asynchronously.  Up to buf_len bytes will be written from buf. (In
  // other words, partial writes are allowed.)  Returns the number of
  // bytes written, or an error code if the operation could not be
  // performed.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the write could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Write() was called when
  // the write has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous write in progress.  That will cancel the write and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened READ_ONLY.
  //
  // Zero byte writes are not allowed.
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback);

  // Forces out a filesystem sync on this file to make sure that the file was
  // written out to disk and is not currently sitting in the buffer. This does
  // not have to be called, it just forces one to happen at the time of
  // calling.
  //
  // The file must be opened with PLATFORM_FILE_ASYNC, and a non-null
  // callback must be passed to this method. If the write could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Flush() was called when
  // the write has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous flush in progress.  That will cancel the flush and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  virtual int Flush(const CompletionCallback& callback);

  // Returns the underlying platform file for testing.
  const base::File& GetFileForTesting() const;

 private:
  class Context;

  net::BoundNetLog bound_net_log_;

  // Context performing I/O operations. It was extracted into a separate class
  // to perform asynchronous operations because FileStream can be destroyed
  // before completion of an async operation. Also if a FileStream is destroyed
  // without explicitly calling Close, the file should be closed asynchronously
  // without delaying FileStream's destructor.
  scoped_ptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(FileStream);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_H_
