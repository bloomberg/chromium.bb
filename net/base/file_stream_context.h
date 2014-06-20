// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream::Context class.
// The general design of FileStream is as follows: file_stream.h defines
// FileStream class which basically is just an "wrapper" not containing any
// specific implementation details. It re-routes all its method calls to
// the instance of FileStream::Context (FileStream holds a scoped_ptr to
// FileStream::Context instance). Context was extracted into a different class
// to be able to do and finish all async operations even when FileStream
// instance is deleted. So FileStream's destructor can schedule file
// closing to be done by Context in WorkerPool (or the TaskRunner passed to
// constructor) and then just return (releasing Context pointer from
// scoped_ptr) without waiting for actual closing to complete.
// Implementation of FileStream::Context is divided in two parts: some methods
// and members are platform-independent and some depend on the platform. This
// header file contains the complete definition of Context class including all
// platform-dependent parts (because of that it has a lot of #if-#else
// branching). Implementations of all platform-independent methods are
// located in file_stream_context.cc, and all platform-dependent methods are
// in file_stream_context_{win,posix}.cc. This separation provides better
// readability of Context's code. And we tried to make as much Context code
// platform-independent as possible. So file_stream_context_{win,posix}.cc are
// much smaller than file_stream_context.cc now.

#ifndef NET_BASE_FILE_STREAM_CONTEXT_H_
#define NET_BASE_FILE_STREAM_CONTEXT_H_

#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/move.h"
#include "base/task_runner.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"

#if defined(OS_POSIX)
#include <errno.h>
#endif

namespace base {
class FilePath;
}

namespace net {

class IOBuffer;

#if defined(OS_WIN)
class FileStream::Context : public base::MessageLoopForIO::IOHandler {
#elif defined(OS_POSIX)
class FileStream::Context {
#endif
 public:
  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  explicit Context(const scoped_refptr<base::TaskRunner>& task_runner);
  Context(base::File file, const scoped_refptr<base::TaskRunner>& task_runner);
#if defined(OS_WIN)
  virtual ~Context();
#elif defined(OS_POSIX)
  ~Context();
#endif

  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback);

  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback);

  const base::File& file() const { return file_; }
  bool async_in_progress() const { return async_in_progress_; }

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Destroys the context. It can be deleted in the method or deletion can be
  // deferred if some asynchronous operation is now in progress or if file is
  // not closed yet.
  void Orphan();

  void Open(const base::FilePath& path,
            int open_flags,
            const CompletionCallback& callback);

  void Close(const CompletionCallback& callback);

  void Seek(base::File::Whence whence,
            int64 offset,
            const Int64CompletionCallback& callback);

  void Flush(const CompletionCallback& callback);

 private:
  struct IOResult {
    IOResult();
    IOResult(int64 result, int os_error);
    static IOResult FromOSError(int64 os_error);

    int64 result;
    int os_error;  // Set only when result < 0.
  };

  struct OpenResult {
    MOVE_ONLY_TYPE_FOR_CPP_03(OpenResult, RValue)
   public:
    OpenResult();
    OpenResult(base::File file, IOResult error_code);
    // C++03 move emulation of this type.
    OpenResult(RValue other);
    OpenResult& operator=(RValue other);

    base::File file;
    IOResult error_code;
  };

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  OpenResult OpenFileImpl(const base::FilePath& path, int open_flags);

  IOResult CloseFileImpl();

  IOResult FlushFileImpl();

  void OnOpenCompleted(const CompletionCallback& callback,
                       OpenResult open_result);

  void CloseAndDelete();

  Int64CompletionCallback IntToInt64(const CompletionCallback& callback);

  // Called when Open() or Seek() completes. |result| contains the result or a
  // network error code.
  void OnAsyncCompleted(const Int64CompletionCallback& callback,
                        const IOResult& result);

  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Adjusts the position from where the data is read.
  IOResult SeekFileImpl(base::File::Whence whence, int64 offset);

  void OnFileOpened();

#if defined(OS_WIN)
  void IOCompletionIsPending(const CompletionCallback& callback, IOBuffer* buf);

  // Implementation of MessageLoopForIO::IOHandler.
  virtual void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_read,
                             DWORD error) OVERRIDE;
#elif defined(OS_POSIX)
  // ReadFileImpl() is a simple wrapper around read() that handles EINTR
  // signals and calls RecordAndMapError() to map errno to net error codes.
  IOResult ReadFileImpl(scoped_refptr<IOBuffer> buf, int buf_len);

  // WriteFileImpl() is a simple wrapper around write() that handles EINTR
  // signals and calls MapSystemError() to map errno to net error codes.
  // It tries to write to completion.
  IOResult WriteFileImpl(scoped_refptr<IOBuffer> buf, int buf_len);
#endif

  base::File file_;
  bool async_in_progress_;
  bool orphaned_;
  scoped_refptr<base::TaskRunner> task_runner_;

#if defined(OS_WIN)
  base::MessageLoopForIO::IOContext io_context_;
  CompletionCallback callback_;
  scoped_refptr<IOBuffer> in_flight_buf_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_CONTEXT_H_
