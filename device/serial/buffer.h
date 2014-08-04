// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_BUFFER_H_
#define DEVICE_SERIAL_BUFFER_H_

#include "base/basictypes.h"

namespace device {

// A fixed-size read-only buffer. The data-reader should call Done() when it is
// finished reading bytes from the buffer. Alternatively, the reader can report
// an error by calling DoneWithError() with the number of bytes read and the
// error it wishes to report.
class ReadOnlyBuffer {
 public:
  virtual ~ReadOnlyBuffer();
  virtual const char* GetData() = 0;
  virtual uint32_t GetSize() = 0;
  virtual void Done(uint32_t bytes_read) = 0;
  virtual void DoneWithError(uint32_t bytes_read, int32_t error) = 0;
};

// A fixed-size writable buffer. The data-writer should call Done() when it is
// finished writing bytes to the buffer. Alternatively, the writer can report
// an error by calling DoneWithError() with the number of bytes written and the
// error it wishes to report.
class WritableBuffer {
 public:
  virtual ~WritableBuffer();
  virtual char* GetData() = 0;
  virtual uint32_t GetSize() = 0;
  virtual void Done(uint32_t bytes_written) = 0;
  virtual void DoneWithError(uint32_t bytes_written, int32_t error) = 0;
};

}  // namespace device

#endif  // DEVICE_SERIAL_BUFFER_H_
