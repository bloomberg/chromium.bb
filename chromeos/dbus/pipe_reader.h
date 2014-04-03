// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PIPE_READER_H_
#define CHROMEOS_DBUS_PIPE_READER_H_

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"

namespace chromeos {

// Simple class to encapsulate collecting data from a pipe into a
// string.  To use:
//   - Instantiate the appropriate subclass of PipeReader
//   - Call StartIO() which will create the appropriate FDs.
//   - Call GetWriteFD() which will return a file descriptor that can
//     be sent to a separate process which will write data there.
//   - After handing off the FD, call CloseWriteFD() so there is
//     only one copy of the FD open.
//   - As data is received, the PipeReader will collect this data
//     as appropriate to the subclass.
//   - When the there is no more data to read, the PipeReader calls
//     |callback|.
class PipeReader {
 public:
  typedef base::Callback<void(void)>IOCompleteCallback;

  explicit PipeReader(PipeReader::IOCompleteCallback callback);
  virtual ~PipeReader();

  // Closes writeable descriptor; normally used in parent process after fork.
  void CloseWriteFD();

  // Starts data collection.  Returns true if stream was setup correctly.
  // On success data will automatically be accumulated into a string that
  // can be retrieved with PipeReader::data().  To shutdown collection delete
  // the instance and/or use PipeReader::OnDataReady(-1).
  bool StartIO();

  // Called when pipe data are available.  Can also be used to shutdown
  // data collection by passing -1 for |byte_count|.
  void OnDataReady(int byte_count);

  // Virtual function that subclasses will override in order to deal
  // with incoming data.
  virtual void AcceptData(const char *data, int length) = 0;

  // Getter for |write_fd_|.
  int write_fd() const { return write_fd_; }

 private:
  int write_fd_;
  scoped_ptr<net::FileStream> data_stream_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  IOCompleteCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PipeReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipeReader);
};

// PipeReader subclass which accepts incoming data to a string.
class PipeReaderForString : public PipeReader {
 public:
  explicit PipeReaderForString(PipeReader::IOCompleteCallback callback);

  virtual void AcceptData(const char *data, int length) OVERRIDE;

  // Destructively returns collected data, by swapping |data_| with |data|.
  void GetData(std::string* data);

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(PipeReaderForString);
};

}

#endif  // CHROMEOS_DBUS_PIPE_READER_H_
