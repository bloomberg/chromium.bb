// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PIPE_READER_H_
#define CHROMEOS_DBUS_PIPE_READER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class TaskRunner;
}

namespace net {
class FileStream;
class IOBufferWithSize;
}

namespace chromeos {

// Simple class to encapsulate collecting data from a pipe into a
// string.  To use:
//   - Instantiate the appropriate subclass of PipeReader
//   - Call StartIO() which will create the appropriate FDs.
//   - As data is received, the PipeReader will collect this data
//     as appropriate to the subclass.
//   - When the there is no more data to read, the PipeReader calls
//     |callback|.
class CHROMEOS_EXPORT PipeReader {
 public:
  typedef base::Callback<void(void)> IOCompleteCallback;

  PipeReader(const scoped_refptr<base::TaskRunner>& task_runner,
             const IOCompleteCallback& callback);
  virtual ~PipeReader();

  // Starts data collection.
  // Returns the write end of the pipe if stream was setup correctly.
  // On success data will automatically be accumulated into a string that
  // can be retrieved with PipeReader::data().  To shutdown collection delete
  // the instance and/or use PipeReader::OnDataReady(-1).
  base::ScopedFD StartIO();

  // Called when pipe data are available.  Can also be used to shutdown
  // data collection by passing -1 for |byte_count|.
  void OnDataReady(int byte_count);

  // Virtual function that subclasses will override in order to deal
  // with incoming data.
  virtual void AcceptData(const char *data, int length) = 0;

 private:
  std::unique_ptr<net::FileStream> data_stream_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  scoped_refptr<base::TaskRunner> task_runner_;
  IOCompleteCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PipeReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipeReader);
};

// PipeReader subclass which accepts incoming data to a string.
class CHROMEOS_EXPORT PipeReaderForString : public PipeReader {
 public:
  PipeReaderForString(const scoped_refptr<base::TaskRunner>& task_runner,
                      const IOCompleteCallback& callback);

  void AcceptData(const char* data, int length) override;

  // Destructively returns collected data, by swapping |data_| with |data|.
  void GetData(std::string* data);

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(PipeReaderForString);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PIPE_READER_H_
